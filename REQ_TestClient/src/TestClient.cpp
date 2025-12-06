#include "../include/req/testclient/TestClient.h"

#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <algorithm>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::testclient {

namespace {
    using Tcp = boost::asio::ip::tcp;
    using ByteArray = std::vector<std::uint8_t>;
    
    // Client version constant
    constexpr const char* CLIENT_VERSION = "REQ-TestClient-0.1";
    
    // Default credentials for quick testing
    constexpr const char* DEFAULT_USERNAME = "testuser";
    constexpr const char* DEFAULT_PASSWORD = "testpass";
    constexpr const char* DEFAULT_MODE = "login";
    
    // Helper: Prompt for input with default value
    std::string promptWithDefault(const std::string& prompt, const std::string& defaultValue) {
        std::cout << prompt;
        std::string input;
        std::getline(std::cin, input);
        
        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t\n\r"));
        input.erase(input.find_last_not_of(" \t\n\r") + 1);
        
        if (input.empty()) {
            return defaultValue;
        }
        return input;
    }
    
    // Helper: Convert mode string to LoginMode enum
    req::shared::protocol::LoginMode parseModeString(const std::string& modeStr) {
        std::string modeLower = modeStr;
        std::transform(modeLower.begin(), modeLower.end(), modeLower.begin(), ::tolower);
        
        if (modeLower == "register" || modeLower == "reg" || modeLower == "r") {
            return req::shared::protocol::LoginMode::Register;
        }
        
        // Default to Login for anything else
        return req::shared::protocol::LoginMode::Login;
    }

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
    req::shared::logInfo("TestClient", "=== REQ Backend Test Client ===");
    req::shared::logInfo("TestClient", "");
    
    // Interactive login prompts
    std::cout << "\n--- Login Information ---\n";
    std::string username = promptWithDefault(
        std::string{"Enter username (default: "} + DEFAULT_USERNAME + "): ",
        DEFAULT_USERNAME
    );
    
    std::string password = promptWithDefault(
        std::string{"Enter password (default: "} + DEFAULT_PASSWORD + "): ",
        DEFAULT_PASSWORD
    );
    
    std::string modeInput = promptWithDefault(
        std::string{"Mode [login/register] (default: "} + DEFAULT_MODE + "): ",
        DEFAULT_MODE
    );
    
    // Parse mode
    req::shared::protocol::LoginMode mode = parseModeString(modeInput);
    std::string modeStr = (mode == req::shared::protocol::LoginMode::Register) ? "register" : "login";
    
    // Log what we're about to do (but not the password!)
    if (mode == req::shared::protocol::LoginMode::Register) {
        req::shared::logInfo("TestClient", std::string{"Registering new account: username="} + username);
    } else {
        req::shared::logInfo("TestClient", std::string{"Logging in with existing account: username="} + username);
    }
    req::shared::logInfo("TestClient", std::string{"Mode: "} + modeStr);
    req::shared::logInfo("TestClient", "");
    
    req::shared::SessionToken sessionToken = 0;
    req::shared::WorldId worldId = 0;
    std::string worldHost;
    std::uint16_t worldPort = 0;
    
    req::shared::HandoffToken handoffToken = 0;
    req::shared::ZoneId zoneId = 0;
    std::string zoneHost;
    std::uint16_t zonePort = 0;

    // Stage 1: Login
    req::shared::logInfo("TestClient", "--- Stage 1: Login/Registration ---");
    if (!doLogin(username, password, CLIENT_VERSION, mode, sessionToken, worldId, worldHost, worldPort)) {
        req::shared::logError("TestClient", "Login stage failed");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return;
    }
    
    if (mode == req::shared::protocol::LoginMode::Register) {
        req::shared::logInfo("TestClient", "Registration and login succeeded!");
    } else {
        req::shared::logInfo("TestClient", "Login succeeded!");
    }
    req::shared::logInfo("TestClient", std::string{"  sessionToken="} + std::to_string(sessionToken));
    req::shared::logInfo("TestClient", std::string{"  worldId="} + std::to_string(worldId));
    req::shared::logInfo("TestClient", std::string{"  worldEndpoint="} + worldHost + ":" + std::to_string(worldPort));

    // Stage 2: Character List
    req::shared::logInfo("TestClient", "--- Stage 2: Character List ---");
    std::vector<req::shared::protocol::CharacterListEntry> characters;
    if (!doCharacterList(worldHost, worldPort, sessionToken, worldId, characters)) {
        req::shared::logError("TestClient", "Character list stage failed");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return;
    }
    req::shared::logInfo("TestClient", std::string{"Character list retrieved: "} + std::to_string(characters.size()) + " character(s)");
    
    // Display characters
    if (characters.empty()) {
        req::shared::logInfo("TestClient", "No characters found. Creating a new character...");
        
        // Create a test character
        req::shared::protocol::CharacterListEntry newChar;
        if (!doCharacterCreate(worldHost, worldPort, sessionToken, worldId, "TestWarrior", "Human", "Warrior", newChar)) {
            req::shared::logError("TestClient", "Character creation failed");
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return;
        }
        
        req::shared::logInfo("TestClient", std::string{"Character created: id="} + std::to_string(newChar.characterId) + 
            ", name=" + newChar.name + ", race=" + newChar.race + ", class=" + newChar.characterClass + 
            ", level=" + std::to_string(newChar.level));
        
        // Add to list
        characters.push_back(newChar);
    } else {
        for (const auto& ch : characters) {
            req::shared::logInfo("TestClient", std::string{"  Character: id="} + std::to_string(ch.characterId) + 
                ", name=" + ch.name + ", race=" + ch.race + ", class=" + ch.characterClass + 
                ", level=" + std::to_string(ch.level));
        }
    }

    // Stage 3: Enter World with Character
    req::shared::logInfo("TestClient", "--- Stage 3: Enter World ---");
    std::uint64_t selectedCharacterId = characters[0].characterId;
    req::shared::logInfo("TestClient", std::string{"Selecting character: id="} + std::to_string(selectedCharacterId) + 
        ", name=" + characters[0].name);
    
    if (!doEnterWorld(worldHost, worldPort, sessionToken, worldId, selectedCharacterId, 
                      handoffToken, zoneId, zoneHost, zonePort)) {
        req::shared::logError("TestClient", "Enter world stage failed");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return;
    }
    req::shared::logInfo("TestClient", std::string{"Enter world succeeded:"});
    req::shared::logInfo("TestClient", std::string{"  handoffToken="} + std::to_string(handoffToken));
    req::shared::logInfo("TestClient", std::string{"  zoneId="} + std::to_string(zoneId));
    req::shared::logInfo("TestClient", std::string{"  zoneEndpoint="} + zoneHost + ":" + std::to_string(zonePort));

    // Stage 4: Zone Auth and Connect
    req::shared::logInfo("TestClient", "--- Stage 4: Zone Auth & Movement Test ---");
    std::shared_ptr<boost::asio::io_context> zoneIoContext;
    std::shared_ptr<Tcp::socket> zoneSocket;
    
    if (!doZoneAuthAndConnect(zoneHost, zonePort, handoffToken, selectedCharacterId,
                             zoneIoContext, zoneSocket)) {
        req::shared::logError("TestClient", "Zone auth stage failed");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return;
    }
    
    req::shared::logInfo("TestClient", "");
    req::shared::logInfo("TestClient", "=== Zone Auth Completed Successfully ===");
    
    // Stage 5: Movement Test Loop
    runMovementTestLoop(zoneIoContext, zoneSocket, selectedCharacterId);
    
    req::shared::logInfo("TestClient", "");
    req::shared::logInfo("TestClient", "=== Test Client Exiting ===");
}

bool TestClient::doLogin(const std::string& username,
                         const std::string& password,
                         const std::string& clientVersion,
                         req::shared::protocol::LoginMode mode,
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
    
    // Build and send LoginRequest with mode
    std::string modeStr = (mode == req::shared::protocol::LoginMode::Register) ? "register" : "login";
    std::string requestPayload = req::shared::protocol::buildLoginRequestPayload(username, password, clientVersion, mode);
    
    req::shared::logInfo("TestClient", std::string{"Sending LoginRequest: username="} + username + 
        ", clientVersion=" + clientVersion + ", mode=" + modeStr);
    
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
        // Log the error with context about what we were trying to do
        if (mode == req::shared::protocol::LoginMode::Register) {
            req::shared::logError("TestClient", std::string{"Registration failed: "} + 
                response.errorCode + " - " + response.errorMessage);
        } else {
            req::shared::logError("TestClient", std::string{"Login failed: "} + 
                response.errorCode + " - " + response.errorMessage);
        }
        return false;
    }
    
    // NEW: Store admin status
    isAdmin_ = response.isAdmin;
    if (isAdmin_) {
        req::shared::logInfo("TestClient", "Logged in as ADMIN account");
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

} // namespace req::testclient
