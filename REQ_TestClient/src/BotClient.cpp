#include "../include/req/testclient/BotClient.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>
#include <array>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::testclient {

namespace {
    constexpr const char* CLIENT_VERSION = "REQ-BotClient-0.1";
    constexpr float MOVEMENT_SEND_INTERVAL_MS = 100.0f;  // Send movement every 100ms
}

BotClient::BotClient(int botIndex)
    : botIndex_(botIndex) {
    startTime_ = std::chrono::steady_clock::now();
    lastTickTime_ = startTime_;
    lastMovementTime_ = startTime_;
}

BotClient::~BotClient() {
    stop();
}

void BotClient::start(const BotConfig& cfg) {
    config_ = cfg;
    running_ = true;
    
    logMinimal("Starting bot");
    
    // Execute handshake sequence
    if (!doLogin()) {
        logMinimal("Login failed, bot stopping");
        running_ = false;
        return;
    }
    
    logMinimal("Logged in successfully");
    authenticated_ = true;
    
    if (!doCharacterList()) {
        logMinimal("Character list failed, bot stopping");
        running_ = false;
        return;
    }
    
    // Character creation handled in doCharacterList if needed
    
    if (!doEnterWorld()) {
        logMinimal("Enter world failed, bot stopping");
        running_ = false;
        return;
    }
    
    logMinimal("Entered world, connecting to zone");
    
    if (!doZoneAuth()) {
        logMinimal("Zone auth failed, bot stopping");
        running_ = false;
        return;
    }
    
    logMinimal("Zone auth successful, bot is now active in zone");
    inZone_ = true;
    
    // Set initial center point (spawn position)
    centerX_ = posX_;
    centerY_ = posY_;
}

void BotClient::tick() {
    if (!running_ || !inZone_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastTickTime_).count();
    lastTickTime_ = now;
    
    // Try to receive any messages from zone server
    if (zoneSocket_ && zoneSocket_->is_open()) {
        req::shared::MessageHeader header;
        std::string msgBody;
        while (tryReceiveMessage(*zoneSocket_, header, msgBody)) {
            if (header.type == req::shared::MessageType::PlayerStateSnapshot) {
                req::shared::protocol::PlayerStateSnapshotData snapshot;
                if (req::shared::protocol::parsePlayerStateSnapshotPayload(msgBody, snapshot)) {
                    handleSnapshot(snapshot);
                } else {
                    logDebug("Failed to parse snapshot");
                }
            } else {
                logDebug("Received unexpected message type: " + std::to_string(static_cast<int>(header.type)));
            }
        }
    }
    
    // Update and send movement
    float timeSinceLastMovement = std::chrono::duration<float>(now - lastMovementTime_).count() * 1000.0f;
    if (timeSinceLastMovement >= MOVEMENT_SEND_INTERVAL_MS) {
        updateMovement(timeSinceLastMovement / 1000.0f);
        lastMovementTime_ = now;
    }
}

void BotClient::stop() {
    if (!running_) {
        return;
    }
    
    logMinimal("Stopping bot");
    running_ = false;
    inZone_ = false;
    
    // Close zone socket gracefully
    if (zoneSocket_ && zoneSocket_->is_open()) {
        boost::system::error_code ec;
        zoneSocket_->shutdown(Tcp::socket::shutdown_both, ec);
        zoneSocket_->close(ec);
    }
}

void BotClient::handleSnapshot(const req::shared::protocol::PlayerStateSnapshotData& snapshot) {
    logNormal("Snapshot " + std::to_string(snapshot.snapshotId) + ": " + 
              std::to_string(snapshot.players.size()) + " player(s)");
    
    // Find our character and update position
    for (const auto& player : snapshot.players) {
        if (player.characterId == characterId_) {
            posX_ = player.posX;
            posY_ = player.posY;
            posZ_ = player.posZ;
            
            logDebug("My position: (" + std::to_string(posX_) + ", " + 
                    std::to_string(posY_) + ", " + std::to_string(posZ_) + ")");
        }
    }
    
    // Debug: log all players if in debug mode
    if (config_.logLevel == BotConfig::LogLevel::Debug) {
        std::ostringstream oss;
        oss << "Snapshot " << snapshot.snapshotId << " players:";
        for (const auto& player : snapshot.players) {
            oss << " [" << player.characterId << "]";
            if (player.characterId == characterId_) {
                oss << "(me)";
            }
        }
        logDebug(oss.str());
    }
}

bool BotClient::doLogin() {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    logDebug("Connecting to login server at 127.0.0.1:7777");
    socket.connect({ boost::asio::ip::make_address("127.0.0.1", ec), 7777 }, ec);
    if (ec) {
        logDebug("Failed to connect to login server: " + ec.message());
        return false;
    }
    
    // Try to login, if that fails, try to register
    std::string requestPayload = req::shared::protocol::buildLoginRequestPayload(
        config_.username, config_.password, CLIENT_VERSION, req::shared::protocol::LoginMode::Login);
    
    if (!sendMessage(socket, req::shared::MessageType::LoginRequest, requestPayload)) {
        return false;
    }
    
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::LoginResponse) {
        logDebug("Unexpected message type from login server");
        return false;
    }
    
    req::shared::protocol::LoginResponseData response;
    if (!req::shared::protocol::parseLoginResponsePayload(respBody, response)) {
        logDebug("Failed to parse LoginResponse");
        return false;
    }
    
    // If login failed, try to register
    if (!response.success) {
        logDebug("Login failed (" + response.errorCode + "), attempting registration");
        
        // Close and reconnect
        socket.close();
        socket = Tcp::socket(io);
        socket.connect({ boost::asio::ip::make_address("127.0.0.1", ec), 7777 }, ec);
        if (ec) {
            logDebug("Failed to reconnect to login server: " + ec.message());
            return false;
        }
        
        // Send registration request
        requestPayload = req::shared::protocol::buildLoginRequestPayload(
            config_.username, config_.password, CLIENT_VERSION, req::shared::protocol::LoginMode::Register);
        
        if (!sendMessage(socket, req::shared::MessageType::LoginRequest, requestPayload)) {
            return false;
        }
        
        if (!receiveMessage(socket, header, respBody)) {
            return false;
        }
        
        if (!req::shared::protocol::parseLoginResponsePayload(respBody, response)) {
            logDebug("Failed to parse registration response");
            return false;
        }
        
        if (!response.success) {
            logDebug("Registration failed: " + response.errorCode + " - " + response.errorMessage);
            return false;
        }
        
        logDebug("Registration successful");
    }
    
    if (response.worlds.empty()) {
        logDebug("No worlds available");
        return false;
    }
    
    // Select first world (or target world if specified)
    const auto& world = response.worlds[0];
    sessionToken_ = response.sessionToken;
    worldId_ = world.worldId;
    worldHost_ = world.worldHost;
    worldPort_ = world.worldPort;
    
    logDebug("Selected world: " + world.worldName);
    
    return true;
}

bool BotClient::doCharacterList() {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    logDebug("Connecting to world server at " + worldHost_ + ":" + std::to_string(worldPort_));
    socket.connect({ boost::asio::ip::make_address(worldHost_, ec), worldPort_ }, ec);
    if (ec) {
        logDebug("Failed to connect to world server: " + ec.message());
        return false;
    }
    
    std::string requestPayload = req::shared::protocol::buildCharacterListRequestPayload(sessionToken_, worldId_);
    if (!sendMessage(socket, req::shared::MessageType::CharacterListRequest, requestPayload)) {
        return false;
    }
    
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::CharacterListResponse) {
        logDebug("Unexpected message type from world server");
        return false;
    }
    
    req::shared::protocol::CharacterListResponseData response;
    if (!req::shared::protocol::parseCharacterListResponsePayload(respBody, response)) {
        logDebug("Failed to parse CharacterListResponse");
        return false;
    }
    
    if (!response.success) {
        logDebug("Character list failed: " + response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    // If no characters, create one
    if (response.characters.empty()) {
        logDebug("No characters found, creating one");
        if (!doCharacterCreate()) {
            return false;
        }
    } else {
        // Use first character
        characterId_ = response.characters[0].characterId;
        logDebug("Using existing character: id=" + std::to_string(characterId_) + 
                ", name=" + response.characters[0].name);
    }
    
    return true;
}

bool BotClient::doCharacterCreate() {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    logDebug("Connecting to world server at " + worldHost_ + ":" + std::to_string(worldPort_));
    socket.connect({ boost::asio::ip::make_address(worldHost_, ec), worldPort_ }, ec);
    if (ec) {
        logDebug("Failed to connect to world server: " + ec.message());
        return false;
    }
    
    // Create character with bot-specific name
    std::string charName = config_.username + "Char";
    std::string requestPayload = req::shared::protocol::buildCharacterCreateRequestPayload(
        sessionToken_, worldId_, charName, "Human", "Warrior");
    
    if (!sendMessage(socket, req::shared::MessageType::CharacterCreateRequest, requestPayload)) {
        return false;
    }
    
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::CharacterCreateResponse) {
        logDebug("Unexpected message type from world server");
        return false;
    }
    
    req::shared::protocol::CharacterCreateResponseData response;
    if (!req::shared::protocol::parseCharacterCreateResponsePayload(respBody, response)) {
        logDebug("Failed to parse CharacterCreateResponse");
        return false;
    }
    
    if (!response.success) {
        logDebug("Character creation failed: " + response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    characterId_ = response.characterId;
    logDebug("Character created: id=" + std::to_string(characterId_) + ", name=" + response.name);
    
    return true;
}

bool BotClient::doEnterWorld() {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    logDebug("Connecting to world server at " + worldHost_ + ":" + std::to_string(worldPort_));
    socket.connect({ boost::asio::ip::make_address(worldHost_, ec), worldPort_ }, ec);
    if (ec) {
        logDebug("Failed to connect to world server: " + ec.message());
        return false;
    }
    
    std::string requestPayload = req::shared::protocol::buildEnterWorldRequestPayload(
        sessionToken_, worldId_, characterId_);
    
    if (!sendMessage(socket, req::shared::MessageType::EnterWorldRequest, requestPayload)) {
        return false;
    }
    
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::EnterWorldResponse) {
        logDebug("Unexpected message type from world server");
        return false;
    }
    
    req::shared::protocol::EnterWorldResponseData response;
    if (!req::shared::protocol::parseEnterWorldResponsePayload(respBody, response)) {
        logDebug("Failed to parse EnterWorldResponse");
        return false;
    }
    
    if (!response.success) {
        logDebug("Enter world failed: " + response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    handoffToken_ = response.handoffToken;
    zoneId_ = response.zoneId;
    zoneHost_ = response.zoneHost;
    zonePort_ = response.zonePort;
    
    logDebug("Handoff to zone: id=" + std::to_string(zoneId_) + 
            ", endpoint=" + zoneHost_ + ":" + std::to_string(zonePort_));
    
    return true;
}

bool BotClient::doZoneAuth() {
    // Create persistent io_context and socket for zone connection
    ioContext_ = std::make_shared<boost::asio::io_context>();
    zoneSocket_ = std::make_shared<Tcp::socket>(*ioContext_);
    
    boost::system::error_code ec;
    
    logDebug("Connecting to zone server at " + zoneHost_ + ":" + std::to_string(zonePort_));
    zoneSocket_->connect({ boost::asio::ip::make_address(zoneHost_, ec), zonePort_ }, ec);
    if (ec) {
        logDebug("Failed to connect to zone server: " + ec.message());
        return false;
    }
    
    std::string requestPayload = req::shared::protocol::buildZoneAuthRequestPayload(handoffToken_, characterId_);
    if (!sendMessage(*zoneSocket_, req::shared::MessageType::ZoneAuthRequest, requestPayload)) {
        return false;
    }
    
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(*zoneSocket_, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::ZoneAuthResponse) {
        logDebug("Unexpected message type from zone server");
        return false;
    }
    
    req::shared::protocol::ZoneAuthResponseData response;
    if (!req::shared::protocol::parseZoneAuthResponsePayload(respBody, response)) {
        logDebug("Failed to parse ZoneAuthResponse");
        return false;
    }
    
    if (!response.success) {
        logDebug("Zone auth failed: " + response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    logDebug("Zone entry: " + response.welcomeMessage);
    
    return true;
}

bool BotClient::sendMessage(Tcp::socket& socket, req::shared::MessageType type, const std::string& body) {
    req::shared::MessageHeader header;
    header.protocolVersion = req::shared::CurrentProtocolVersion;
    header.type = type;
    header.payloadSize = static_cast<std::uint32_t>(body.size());
    header.reserved = 0;
    
    ByteArray payloadBytes(body.begin(), body.end());
    std::array<boost::asio::const_buffer, 2> buffers = {
        boost::asio::buffer(&header, sizeof(header)),
        boost::asio::buffer(payloadBytes)
    };
    
    boost::system::error_code ec;
    boost::asio::write(socket, buffers, ec);
    if (ec) {
        logDebug("Failed to send message: " + ec.message());
        return false;
    }
    return true;
}

bool BotClient::receiveMessage(Tcp::socket& socket, req::shared::MessageHeader& outHeader, std::string& outBody) {
    boost::system::error_code ec;
    boost::asio::read(socket, boost::asio::buffer(&outHeader, sizeof(outHeader)), ec);
    if (ec) {
        logDebug("Failed to read header: " + ec.message());
        return false;
    }
    
    ByteArray bodyBytes;
    bodyBytes.resize(outHeader.payloadSize);
    if (outHeader.payloadSize > 0) {
        boost::asio::read(socket, boost::asio::buffer(bodyBytes), ec);
        if (ec) {
            logDebug("Failed to read body: " + ec.message());
            return false;
        }
    }
    outBody.assign(bodyBytes.begin(), bodyBytes.end());
    return true;
}

bool BotClient::tryReceiveMessage(Tcp::socket& socket, req::shared::MessageHeader& outHeader, std::string& outBody) {
    socket.non_blocking(true);
    
    boost::system::error_code ec;
    std::size_t bytesRead = socket.read_some(
        boost::asio::buffer(&outHeader, sizeof(outHeader)), ec);
    
    if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
        socket.non_blocking(false);
        return false;
    }
    
    if (ec || bytesRead != sizeof(outHeader)) {
        socket.non_blocking(false);
        return false;
    }
    
    ByteArray bodyBytes;
    bodyBytes.resize(outHeader.payloadSize);
    if (outHeader.payloadSize > 0) {
        boost::asio::read(socket, boost::asio::buffer(bodyBytes), ec);
        if (ec) {
            socket.non_blocking(false);
            return false;
        }
    }
    
    socket.non_blocking(false);
    outBody.assign(bodyBytes.begin(), bodyBytes.end());
    return true;
}

} // namespace req::testclient
