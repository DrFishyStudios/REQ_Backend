// Negative/error test scenario implementations for REQ_TestClient
// Tests error handling and security validation

#include "../include/req/testclient/TestClient.h"

#include <iostream>
#include <thread>
#include <chrono>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::testclient {

namespace {
    using Tcp = boost::asio::ip::tcp;
    using ByteArray = std::vector<std::uint8_t>;
    
    // Default test credentials
    constexpr const char* DEFAULT_USERNAME = "testuser";
    constexpr const char* DEFAULT_PASSWORD = "testpass";
    constexpr const char* CLIENT_VERSION = "REQ-TestClient-0.2";
    
    std::string promptWithDefault(const std::string& prompt, const std::string& defaultValue) {
        std::cout << prompt;
        std::string input;
        std::getline(std::cin, input);
        
        input.erase(0, input.find_first_not_of(" \t\n\r"));
        input.erase(input.find_last_not_of(" \t\n\r") + 1);
        
        if (input.empty()) {
            return defaultValue;
        }
        return input;
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

// Bad password test
void TestClient::runBadPasswordTest() {
    req::shared::logInfo("TestClient", "=== BAD PASSWORD TEST ===");
    std::cout << "\n=== Bad Password Test ===\n";
    std::cout << "This test attempts login with incorrect password.\n\n";
    
    std::string username = promptWithDefault("Username: ", DEFAULT_USERNAME);
    std::string correctPassword = promptWithDefault("Correct password: ", DEFAULT_PASSWORD);
    std::string wrongPassword = promptWithDefault("Wrong password to test: ", "wrongpassword");
    
    transitionStage(EClientStage::LoginPending, "username=" + username + ", password=<wrong>");
    
    req::shared::SessionToken sessionToken = 0;
    req::shared::WorldId worldId = 0;
    std::string worldHost;
    std::uint16_t worldPort = 0;
    
    // Attempt login with wrong password
    bool result = doLogin(username, wrongPassword, CLIENT_VERSION, 
                         req::shared::protocol::LoginMode::Login,
                         sessionToken, worldId, worldHost, worldPort);
    
    if (result) {
        // This shouldn't happen - server accepted bad password
        transitionStage(EClientStage::Error, "Server accepted wrong password - SECURITY ISSUE");
        req::shared::logError("TestClient", "? TEST FAILED: Server accepted incorrect password");
        std::cout << "\n? TEST FAILED: Server should have rejected bad password\n";
    } else {
        // Expected failure
        transitionStage(EClientStage::Error, "Login rejected (expected)");
        req::shared::logInfo("TestClient", "? Server correctly rejected bad password");
        std::cout << "\n? TEST PASSED: Server correctly rejected bad password\n";
        std::cout << "Check server logs for error code (should be INVALID_PASSWORD)\n";
    }
    
    std::cout << "\nPress Enter to continue...";
    std::cin.get();
}

// Bad session token test
void TestClient::runBadSessionTokenTest() {
    req::shared::logInfo("TestClient", "=== BAD SESSION TOKEN TEST ===");
    std::cout << "\n=== Bad Session Token Test ===\n";
    std::cout << "This test corrupts the sessionToken before CharacterListRequest.\n\n";
    
    std::string username = promptWithDefault("Username: ", DEFAULT_USERNAME);
    std::string password = promptWithDefault("Password: ", DEFAULT_PASSWORD);
    
    // First, do valid login
    transitionStage(EClientStage::LoginPending, "username=" + username);
    
    req::shared::SessionToken validSessionToken = 0;
    req::shared::WorldId worldId = 0;
    std::string worldHost;
    std::uint16_t worldPort = 0;
    
    if (!doLogin(username, password, CLIENT_VERSION, req::shared::protocol::LoginMode::Login,
                 validSessionToken, worldId, worldHost, worldPort)) {
        transitionStage(EClientStage::Error, "Login failed - cannot proceed with test");
        std::cout << "\n? TEST ABORTED: Login failed\n";
        std::cin.get();
        return;
    }
    
    transitionStage(EClientStage::LoggedIn, "sessionToken=" + std::to_string(validSessionToken));
    
    req::shared::logInfo("TestClient", std::string{"Valid sessionToken: "} + std::to_string(validSessionToken));
    std::cout << "Valid sessionToken obtained: " << validSessionToken << "\n";
    
    // Corrupt the session token
    req::shared::SessionToken corruptedToken = validSessionToken + 99999;
    req::shared::logInfo("TestClient", std::string{"Corrupted sessionToken: "} + std::to_string(corruptedToken));
    std::cout << "Corrupted sessionToken:      " << corruptedToken << " (original + 99999)\n\n";
    
    transitionStage(EClientStage::WorldSelected, "Using corrupted sessionToken");
    
    // Attempt CharacterListRequest with corrupted token
    std::cout << "Sending CharacterListRequest with corrupted token...\n";
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    socket.connect({ boost::asio::ip::make_address(worldHost, ec), worldPort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to world server");
        transitionStage(EClientStage::Error, "Connection failed");
        std::cout << "\n? TEST ABORTED: Cannot connect to world server\n";
        std::cin.get();
        return;
    }
    
    std::string requestPayload = req::shared::protocol::buildCharacterListRequestPayload(corruptedToken, worldId);
    req::shared::logInfo("TestClient", std::string{"Sending CharacterListRequest with corruptedToken="} + 
        std::to_string(corruptedToken));
    
    if (!sendMessage(socket, req::shared::MessageType::CharacterListRequest, requestPayload)) {
        transitionStage(EClientStage::Error, "Failed to send request");
        std::cout << "\n? TEST ABORTED: Failed to send request\n";
        std::cin.get();
        return;
    }
    
    // Receive response
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        transitionStage(EClientStage::Error, "No response - silent failure");
        std::cout << "\n? TEST FAILED: No response from server (silent failure)\n";
        std::cin.get();
        return;
    }
    
    // Parse response
    req::shared::protocol::CharacterListResponseData response;
    if (!req::shared::protocol::parseCharacterListResponsePayload(respBody, response)) {
        transitionStage(EClientStage::Error, "Malformed response");
        std::cout << "\n? TEST FAILED: Cannot parse response\n";
        std::cin.get();
        return;
    }
    
    // Check if server rejected it
    if (response.success) {
        transitionStage(EClientStage::Error, "Server accepted corrupted token - SECURITY ISSUE");
        req::shared::logError("TestClient", "? TEST FAILED: Server accepted corrupted sessionToken");
        std::cout << "\n? TEST FAILED: Server should have rejected corrupted token\n";
    } else {
        transitionStage(EClientStage::Error, "Server rejected corrupted token (expected)");
        req::shared::logInfo("TestClient", std::string{"? Server rejected: errorCode='"} + 
            response.errorCode + "', errorMessage='" + response.errorMessage + "'");
        std::cout << "\n? TEST PASSED: Server correctly rejected corrupted sessionToken\n";
        std::cout << "Error response:\n";
        std::cout << "  errorCode:    " << response.errorCode << "\n";
        std::cout << "  errorMessage: " << response.errorMessage << "\n";
        std::cout << "Expected errorCode: INVALID_SESSION\n";
    }
    
    socket.close(ec);
    
    std::cout << "\nPress Enter to continue...";
    std::cin.get();
}

// Bad handoff token test
void TestClient::runBadHandoffTokenTest() {
    req::shared::logInfo("TestClient", "=== BAD HANDOFF TOKEN TEST ===");
    std::cout << "\n=== Bad Handoff Token Test ===\n";
    std::cout << "This test corrupts the handoffToken before ZoneAuthRequest.\n";
    std::cout << "Requires valid login ? world ? character ? enterWorld first.\n\n";
    
    std::string continueTest = promptWithDefault("Continue with full handshake? (y/n, default: y): ", "y");
    if (continueTest != "y" && continueTest != "Y") {
        std::cout << "Test cancelled.\n";
        return;
    }
    
    std::string username = promptWithDefault("Username: ", DEFAULT_USERNAME);
    std::string password = promptWithDefault("Password: ", DEFAULT_PASSWORD);
    
    // Do full handshake up to EnterWorld
    transitionStage(EClientStage::LoginPending, "username=" + username);
    
    req::shared::SessionToken sessionToken = 0;
    req::shared::WorldId worldId = 0;
    std::string worldHost;
    std::uint16_t worldPort = 0;
    
    if (!doLogin(username, password, CLIENT_VERSION, req::shared::protocol::LoginMode::Login,
                 sessionToken, worldId, worldHost, worldPort)) {
        transitionStage(EClientStage::Error, "Login failed");
        std::cout << "\n? TEST ABORTED: Login failed\n";
        std::cin.get();
        return;
    }
    
    transitionStage(EClientStage::LoggedIn, "sessionToken=" + std::to_string(sessionToken));
    transitionStage(EClientStage::WorldSelected, "worldId=" + std::to_string(worldId));
    
    // Get character list
    std::vector<req::shared::protocol::CharacterListEntry> characters;
    if (!doCharacterList(worldHost, worldPort, sessionToken, worldId, characters)) {
        transitionStage(EClientStage::Error, "Character list failed");
        std::cout << "\n? TEST ABORTED: Character list failed\n";
        std::cin.get();
        return;
    }
    
    if (characters.empty()) {
        std::cout << "No characters - creating one...\n";
        req::shared::protocol::CharacterListEntry newChar;
        if (!doCharacterCreate(worldHost, worldPort, sessionToken, worldId, 
                              "TestWarrior", "Human", "Warrior", newChar)) {
            transitionStage(EClientStage::Error, "Character creation failed");
            std::cout << "\n? TEST ABORTED: Character creation failed\n";
            std::cin.get();
            return;
        }
        characters.push_back(newChar);
    }
    
    transitionStage(EClientStage::CharactersLoaded, "count=" + std::to_string(characters.size()));
    
    std::uint64_t characterId = characters[0].characterId;
    
    // EnterWorld to get handoffToken
    transitionStage(EClientStage::EnteringWorld, "characterId=" + std::to_string(characterId));
    
    req::shared::HandoffToken validHandoffToken = 0;
    req::shared::ZoneId zoneId = 0;
    std::string zoneHost;
    std::uint16_t zonePort = 0;
    
    if (!doEnterWorld(worldHost, worldPort, sessionToken, worldId, characterId,
                     validHandoffToken, zoneId, zoneHost, zonePort)) {
        transitionStage(EClientStage::Error, "EnterWorld failed");
        std::cout << "\n? TEST ABORTED: EnterWorld failed\n";
        std::cin.get();
        return;
    }
    
    req::shared::logInfo("TestClient", std::string{"Valid handoffToken: "} + std::to_string(validHandoffToken));
    std::cout << "Valid handoffToken obtained: " << validHandoffToken << "\n";
    
    // Corrupt the handoff token
    req::shared::HandoffToken corruptedToken = validHandoffToken + 88888;
    req::shared::logInfo("TestClient", std::string{"Corrupted handoffToken: "} + std::to_string(corruptedToken));
    std::cout << "Corrupted handoffToken:      " << corruptedToken << " (original + 88888)\n\n";
    
    // Connect to zone with corrupted token
    std::cout << "Connecting to zone server...\n";
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    socket.connect({ boost::asio::ip::make_address(zoneHost, ec), zonePort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to zone server");
        transitionStage(EClientStage::Error, "Connection failed");
        std::cout << "\n? TEST ABORTED: Cannot connect to zone server\n";
        std::cin.get();
        return;
    }
    
    // Send ZoneAuthRequest with corrupted token
    std::string requestPayload = req::shared::protocol::buildZoneAuthRequestPayload(corruptedToken, characterId);
    req::shared::logInfo("TestClient", std::string{"Sending ZoneAuthRequest with corruptedToken="} + 
        std::to_string(corruptedToken));
    
    if (!sendMessage(socket, req::shared::MessageType::ZoneAuthRequest, requestPayload)) {
        transitionStage(EClientStage::Error, "Failed to send request");
        std::cout << "\n? TEST ABORTED: Failed to send request\n";
        std::cin.get();
        return;
    }
    
    // Receive response
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        transitionStage(EClientStage::Error, "No response - silent failure");
        std::cout << "\n? TEST FAILED: No response from server (silent failure)\n";
        std::cin.get();
        return;
    }
    
    // Parse response
    req::shared::protocol::ZoneAuthResponseData response;
    if (!req::shared::protocol::parseZoneAuthResponsePayload(respBody, response)) {
        transitionStage(EClientStage::Error, "Malformed response");
        std::cout << "\n? TEST FAILED: Cannot parse response\n";
        std::cin.get();
        return;
    }
    
    // Check if server rejected it
    if (response.success) {
        transitionStage(EClientStage::Error, "Server accepted corrupted token - SECURITY ISSUE");
        req::shared::logError("TestClient", "? TEST FAILED: Server accepted corrupted handoffToken");
        std::cout << "\n? TEST FAILED: Server should have rejected corrupted handoffToken\n";
    } else {
        transitionStage(EClientStage::InZone, "Server rejected corrupted token (expected)");
        req::shared::logInfo("TestClient", std::string{"? Server rejected: errorCode='"} + 
            response.errorCode + "', errorMessage='" + response.errorMessage + "'");
        std::cout << "\n? TEST PASSED: Server correctly rejected corrupted handoffToken\n";
        std::cout << "Error response:\n";
        std::cout << "  errorCode:    " << response.errorCode << "\n";
        std::cout << "  errorMessage: " << response.errorMessage << "\n";
        std::cout << "Expected errorCode: INVALID_HANDOFF (stub validation accepts non-zero, future will validate properly)\n";
    }
    
    socket.close(ec);
    
    std::cout << "\nPress Enter to continue...";
    std::cin.get();
}

} // namespace req::testclient
