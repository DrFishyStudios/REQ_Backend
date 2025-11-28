#include "../include/req/testclient/TestClient.h"

#include <iostream>
#include <vector>
#include <array>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::testclient {

namespace {
    using Tcp = boost::asio::ip::tcp;
    using ByteArray = std::vector<std::uint8_t>;

    bool sendMessage(Tcp::socket& socket,
                     req::shared::MessageType type,
                     const std::string& body) {
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
            req::shared::logError("TestClient", "Failed to send message: " + ec.message());
            return false;
        }
        return true;
    }

    bool receiveMessage(Tcp::socket& socket,
                        req::shared::MessageHeader& outHeader,
                        std::string& outBody) {
        boost::system::error_code ec;
        boost::asio::read(socket, boost::asio::buffer(&outHeader, sizeof(outHeader)), ec);
        if (ec) {
            req::shared::logError("TestClient", "Failed to read header: " + ec.message());
            return false;
        }
        
        // Log received header
        req::shared::logInfo("TestClient", std::string{"Received: type="} + 
            std::to_string(static_cast<int>(outHeader.type)) +
            ", protocolVersion=" + std::to_string(outHeader.protocolVersion) +
            ", payloadSize=" + std::to_string(outHeader.payloadSize));
        
        ByteArray bodyBytes;
        bodyBytes.resize(outHeader.payloadSize);
        if (outHeader.payloadSize > 0) {
            boost::asio::read(socket, boost::asio::buffer(bodyBytes), ec);
            if (ec) {
                req::shared::logError("TestClient", "Failed to read body: " + ec.message());
                return false;
            }
        }
        outBody.assign(bodyBytes.begin(), bodyBytes.end());
        return true;
    }
}

void TestClient::run() {
    req::shared::logInfo("TestClient", "=== Starting Login ? World ? Zone Handshake Test ===");
    
    req::shared::SessionToken sessionToken = 0;
    req::shared::WorldId worldId = 0;
    std::string worldHost;
    std::uint16_t worldPort = 0;
    
    req::shared::HandoffToken handoffToken = 0;
    req::shared::ZoneId zoneId = 0;
    std::string zoneHost;
    std::uint16_t zonePort = 0;

    const std::string username = "testuser";
    const std::string password = "testpass";
    const std::string clientVersion = "0.1.0";

    // Stage 1: Login
    req::shared::logInfo("TestClient", "--- Stage 1: Login ---");
    if (!doLogin(username, password, clientVersion, sessionToken, worldId, worldHost, worldPort)) {
        req::shared::logError("TestClient", "Login stage failed");
        return;
    }
    req::shared::logInfo("TestClient", std::string{"Login stage succeeded:"});
    req::shared::logInfo("TestClient", std::string{"  sessionToken="} + std::to_string(sessionToken));
    req::shared::logInfo("TestClient", std::string{"  worldId="} + std::to_string(worldId));
    req::shared::logInfo("TestClient", std::string{"  worldEndpoint="} + worldHost + ":" + std::to_string(worldPort));

    // Stage 2: World Auth
    req::shared::logInfo("TestClient", "--- Stage 2: World Auth ---");
    if (!doWorldAuth(worldHost, worldPort, sessionToken, worldId, handoffToken, zoneId, zoneHost, zonePort)) {
        req::shared::logError("TestClient", "World auth stage failed");
        return;
    }
    req::shared::logInfo("TestClient", std::string{"World auth stage succeeded:"});
    req::shared::logInfo("TestClient", std::string{"  handoffToken="} + std::to_string(handoffToken));
    req::shared::logInfo("TestClient", std::string{"  zoneId="} + std::to_string(zoneId));
    req::shared::logInfo("TestClient", std::string{"  zoneEndpoint="} + zoneHost + ":" + std::to_string(zonePort));

    // Stage 3: Zone Auth
    req::shared::logInfo("TestClient", "--- Stage 3: Zone Auth ---");
    const req::shared::PlayerId testCharacterId = 42; // Hardcoded test character
    if (!doZoneAuth(zoneHost, zonePort, handoffToken, testCharacterId)) {
        req::shared::logError("TestClient", "Zone auth stage failed");
        return;
    }
    
    req::shared::logInfo("TestClient", "=== Full Handshake Completed Successfully ===");
}

bool TestClient::doLogin(const std::string& username,
                         const std::string& password,
                         const std::string& clientVersion,
                         req::shared::SessionToken& outSessionToken,
                         req::shared::WorldId& outWorldId,
                         std::string& outWorldHost,
                         std::uint16_t& outWorldPort) {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    req::shared::logInfo("TestClient", "Connecting to login server at 127.0.0.1:7777...");
    socket.connect({ boost::asio::ip::make_address("127.0.0.1", ec), 7777 }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to login server: " + ec.message());
        return false;
    }
    req::shared::logInfo("TestClient", "Connected to login server");
    
    // Build and send LoginRequest
    std::string requestPayload = req::shared::protocol::buildLoginRequestPayload(username, password, clientVersion);
    req::shared::logInfo("TestClient", std::string{"Sending LoginRequest: username="} + username + 
        ", clientVersion=" + clientVersion);
    
    if (!sendMessage(socket, req::shared::MessageType::LoginRequest, requestPayload)) {
        return false;
    }

    // Receive and parse LoginResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::LoginResponse) {
        req::shared::logError("TestClient", "Unexpected message type from login server");
        return false;
    }
    
    req::shared::protocol::LoginResponseData response;
    if (!req::shared::protocol::parseLoginResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse LoginResponse");
        return false;
    }
    
    if (!response.success) {
        req::shared::logError("TestClient", std::string{"Login failed: "} + response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    if (response.worlds.empty()) {
        req::shared::logError("TestClient", "No worlds available");
        return false;
    }
    
    // Select first world
    const auto& world = response.worlds[0];
    outSessionToken = response.sessionToken;
    outWorldId = world.worldId;
    outWorldHost = world.worldHost;
    outWorldPort = world.worldPort;
    
    req::shared::logInfo("TestClient", std::string{"Selected world: "} + world.worldName + 
        " (ruleset: " + world.rulesetId + ")");
    
    return true;
}

bool TestClient::doWorldAuth(const std::string& worldHost,
                             std::uint16_t worldPort,
                             req::shared::SessionToken sessionToken,
                             req::shared::WorldId worldId,
                             req::shared::HandoffToken& outHandoffToken,
                             req::shared::ZoneId& outZoneId,
                             std::string& outZoneHost,
                             std::uint16_t& outZonePort) {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    req::shared::logInfo("TestClient", std::string{"Connecting to world server at "} + 
        worldHost + ":" + std::to_string(worldPort) + "...");
    socket.connect({ boost::asio::ip::make_address(worldHost, ec), worldPort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to world server: " + ec.message());
        return false;
    }
    req::shared::logInfo("TestClient", "Connected to world server");
    
    // Build and send WorldAuthRequest
    std::string requestPayload = req::shared::protocol::buildWorldAuthRequestPayload(sessionToken, worldId);
    req::shared::logInfo("TestClient", std::string{"Sending WorldAuthRequest: sessionToken="} + 
        std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId));
    
    if (!sendMessage(socket, req::shared::MessageType::WorldAuthRequest, requestPayload)) {
        return false;
    }

    // Receive and parse WorldAuthResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::WorldAuthResponse) {
        req::shared::logError("TestClient", "Unexpected message type from world server");
        return false;
    }
    
    req::shared::protocol::WorldAuthResponseData response;
    if (!req::shared::protocol::parseWorldAuthResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse WorldAuthResponse");
        return false;
    }
    
    if (!response.success) {
        req::shared::logError("TestClient", std::string{"World auth failed: "} + 
            response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    outHandoffToken = response.handoffToken;
    outZoneId = response.zoneId;
    outZoneHost = response.zoneHost;
    outZonePort = response.zonePort;
    
    return true;
}

bool TestClient::doZoneAuth(const std::string& zoneHost,
                            std::uint16_t zonePort,
                            req::shared::HandoffToken handoffToken,
                            req::shared::PlayerId characterId) {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    req::shared::logInfo("TestClient", std::string{"Connecting to zone server at "} + 
        zoneHost + ":" + std::to_string(zonePort) + "...");
    socket.connect({ boost::asio::ip::make_address(zoneHost, ec), zonePort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to zone server: " + ec.message());
        return false;
    }
    req::shared::logInfo("TestClient", "Connected to zone server");
    
    // Build and send ZoneAuthRequest
    std::string requestPayload = req::shared::protocol::buildZoneAuthRequestPayload(handoffToken, characterId);
    req::shared::logInfo("TestClient", std::string{"Sending ZoneAuthRequest: handoffToken="} + 
        std::to_string(handoffToken) + ", characterId=" + std::to_string(characterId));
    
    if (!sendMessage(socket, req::shared::MessageType::ZoneAuthRequest, requestPayload)) {
        return false;
    }

    // Receive and parse ZoneAuthResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::ZoneAuthResponse) {
        req::shared::logError("TestClient", "Unexpected message type from zone server");
        return false;
    }
    
    req::shared::protocol::ZoneAuthResponseData response;
    if (!req::shared::protocol::parseZoneAuthResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse ZoneAuthResponse");
        return false;
    }
    
    if (!response.success) {
        req::shared::logError("TestClient", std::string{"Zone auth failed: "} + 
            response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    req::shared::logInfo("TestClient", std::string{"Zone entry successful: "} + response.welcomeMessage);
    
    return true;
}

} // namespace req::testclient
