// Happy path scenario implementations for REQ_TestClient
// Implements successful end-to-end handshake and basic operations

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
}

// Happy path scenario
void TestClient::runHappyPathScenario() {
    req::shared::logInfo("TestClient", "=== HAPPY PATH SCENARIO ===");
    req::shared::logInfo("TestClient", "Automated end-to-end handshake test");
    std::cout << "\n=== Happy Path Scenario ===\n";
    std::cout << "This will automatically:\n";
    std::cout << "  1. Login to LoginServer\n";
    std::cout << "  2. Select first world\n";
    std::cout << "  3. Load/create character\n";
    std::cout << "  4. Enter world and zone\n";
    std::cout << "  5. Send test movement\n\n";
    
    // Prompt for credentials
    std::string username = promptWithDefault(
        std::string{"Enter username (default: "} + DEFAULT_USERNAME + "): ",
        DEFAULT_USERNAME
    );
    
    std::string password = promptWithDefault(
        std::string{"Enter password (default: "} + DEFAULT_PASSWORD + "): ",
        DEFAULT_PASSWORD
    );
    
    transitionStage(EClientStage::LoginPending, "username=" + username);
    
    // Stage 1: Login
    req::shared::SessionToken sessionToken = 0;
    req::shared::WorldId worldId = 0;
    std::string worldHost;
    std::uint16_t worldPort = 0;
    
    if (!doLogin(username, password, CLIENT_VERSION, req::shared::protocol::LoginMode::Login,
                 sessionToken, worldId, worldHost, worldPort)) {
        transitionStage(EClientStage::Error, "Login failed");
        std::cout << "\n? Happy path FAILED at login stage\n";
        return;
    }
    
    sessionToken_ = sessionToken;
    worldId_ = worldId;
    accountId_ = 1; // Stub
    
    transitionStage(EClientStage::LoggedIn, 
        "sessionToken=" + std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId));
    
    // Stage 2: Select world (automatic - pick first)
    transitionStage(EClientStage::WorldSelected, 
        "worldId=" + std::to_string(worldId) + ", endpoint=" + worldHost + ":" + std::to_string(worldPort));
    
    // Stage 3: Character list
    std::vector<req::shared::protocol::CharacterListEntry> characters;
    if (!doCharacterList(worldHost, worldPort, sessionToken, worldId, characters)) {
        transitionStage(EClientStage::Error, "Character list failed");
        std::cout << "\n? Happy path FAILED at character list stage\n";
        return;
    }
    
    transitionStage(EClientStage::CharactersLoaded, "count=" + std::to_string(characters.size()));
    
    // Stage 4: Handle character creation if needed
    if (characters.empty()) {
        req::shared::logInfo("TestClient", "No characters found - creating default character");
        std::cout << "No characters found. Creating default character (Human Warrior)...\n";
        
        req::shared::protocol::CharacterListEntry newChar;
        if (!doCharacterCreate(worldHost, worldPort, sessionToken, worldId, 
                              "TestWarrior", "Human", "Warrior", newChar)) {
            transitionStage(EClientStage::Error, "Character creation failed");
            std::cout << "\n? Happy path FAILED at character creation\n";
            return;
        }
        
        req::shared::logInfo("TestClient", std::string{"Character created: id="} + 
            std::to_string(newChar.characterId) + ", name=" + newChar.name);
        
        // Refresh character list
        if (!doCharacterList(worldHost, worldPort, sessionToken, worldId, characters)) {
            transitionStage(EClientStage::Error, "Character list refresh failed");
            std::cout << "\n? Happy path FAILED at character list refresh\n";
            return;
        }
    }
    
    // Pick first character
    selectedCharacterId_ = characters[0].characterId;
    req::shared::logInfo("TestClient", std::string{"Selected character: id="} + 
        std::to_string(selectedCharacterId_) + ", name=" + characters[0].name);
    
    // Stage 5: Enter world
    transitionStage(EClientStage::EnteringWorld, 
        "characterId=" + std::to_string(selectedCharacterId_) + ", name=" + characters[0].name);
    
    req::shared::HandoffToken handoffToken = 0;
    req::shared::ZoneId zoneId = 0;
    std::string zoneHost;
    std::uint16_t zonePort = 0;
    
    if (!doEnterWorld(worldHost, worldPort, sessionToken, worldId, selectedCharacterId_,
                     handoffToken, zoneId, zoneHost, zonePort)) {
        transitionStage(EClientStage::Error, "Enter world failed");
        std::cout << "\n? Happy path FAILED at enter world stage\n";
        return;
    }
    
    handoffToken_ = handoffToken;
    zoneId_ = zoneId;
    
    req::shared::logInfo("TestClient", std::string{"Zone handoff received: handoffToken="} + 
        std::to_string(handoffToken) + ", zoneId=" + std::to_string(zoneId) + 
        ", endpoint=" + zoneHost + ":" + std::to_string(zonePort));
    
    // Stage 6: Zone auth
    std::shared_ptr<boost::asio::io_context> zoneIoContext;
    std::shared_ptr<Tcp::socket> zoneSocket;
    
    if (!doZoneAuthAndConnect(zoneHost, zonePort, handoffToken, selectedCharacterId_,
                             zoneIoContext, zoneSocket)) {
        transitionStage(EClientStage::Error, "Zone auth failed");
        std::cout << "\n? Happy path FAILED at zone auth stage\n";
        return;
    }
    
    transitionStage(EClientStage::InZone, 
        "zoneId=" + std::to_string(zoneId) + ", characterId=" + std::to_string(selectedCharacterId_));
    
    // Stage 7: Brief movement test
    req::shared::logInfo("TestClient", "Sending test movement commands...");
    std::cout << "\nSending 3 test movement commands...\n";
    
    // Send 3 simple movement commands
    for (int i = 0; i < 3; ++i) {
        req::shared::protocol::MovementIntentData intent;
        intent.characterId = selectedCharacterId_;
        intent.sequenceNumber = i + 1;
        intent.inputY = 1.0f; // Move forward
        intent.facingYawDegrees = 0.0f;
        intent.isJumpPressed = false;
        intent.clientTimeMs = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        
        std::string payload = req::shared::protocol::buildMovementIntentPayload(intent);
        if (!sendMessage(*zoneSocket, req::shared::MessageType::MovementIntent, payload)) {
            req::shared::logError("TestClient", "Failed to send movement intent");
            break;
        }
        
        req::shared::logInfo("TestClient", std::string{"Sent MovementIntent seq="} + std::to_string(i + 1));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Success summary
    req::shared::logInfo("TestClient", "");
    req::shared::logInfo("TestClient", "=== HAPPY PATH COMPLETE ===");
    req::shared::logInfo("TestClient", std::string{"? Login successful: username="} + username + 
        ", accountId=" + std::to_string(accountId_));
    req::shared::logInfo("TestClient", std::string{"? World selected: worldId="} + std::to_string(worldId_));
    req::shared::logInfo("TestClient", std::string{"? Character selected: characterId="} + 
        std::to_string(selectedCharacterId_) + ", name=" + characters[0].name);
    req::shared::logInfo("TestClient", std::string{"? Zone entered: zoneId="} + std::to_string(zoneId_));
    req::shared::logInfo("TestClient", "? Movement test completed");
    
    std::cout << "\n? HAPPY PATH COMPLETE\n";
    std::cout << "All stages succeeded:\n";
    std::cout << "  Login ? World ? Characters ? EnterWorld ? ZoneAuth ? Movement\n";
    std::cout << "\nKey IDs:\n";
    std::cout << "  accountId (stub):  " << accountId_ << "\n";
    std::cout << "  sessionToken:      " << sessionToken_ << "\n";
    std::cout << "  worldId:           " << worldId_ << "\n";
    std::cout << "  characterId:       " << selectedCharacterId_ << "\n";
    std::cout << "  handoffToken:      " << handoffToken_ << "\n";
    std::cout << "  zoneId:            " << zoneId_ << "\n";
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
}

} // namespace req::testclient
