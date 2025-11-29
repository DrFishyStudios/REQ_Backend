// Movement test loop implementation for TestClient
// This file contains the movement-related code to keep TestClient.cpp cleaner

#include "../include/req/testclient/TestClient.h"

#include <iostream>
#include <chrono>
#include <thread>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::testclient {

namespace {
    using Tcp = boost::asio::ip::tcp;
    using ByteArray = std::vector<std::uint8_t>;
    
    // Movement test timing
    auto g_startTime = std::chrono::steady_clock::now();
    
    std::uint32_t getClientTimeMs() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime);
        return static_cast<std::uint32_t>(duration.count());
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
            if (ec != boost::asio::error::eof && ec != boost::asio::error::would_block) {
                req::shared::logError("TestClient", "Failed to read header: " + ec.message());
            }
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
    
    // Try to receive messages without blocking
    bool tryReceiveMessage(Tcp::socket& socket,
                          req::shared::MessageHeader& outHeader,
                          std::string& outBody) {
        // Set socket to non-blocking mode temporarily
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
        
        // Read body if present
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
}

bool TestClient::doZoneAuthAndConnect(const std::string& zoneHost,
                                     std::uint16_t zonePort,
                                     req::shared::HandoffToken handoffToken,
                                     req::shared::PlayerId characterId,
                                     std::shared_ptr<boost::asio::io_context>& outIoContext,
                                     std::shared_ptr<Tcp::socket>& outSocket) {
    // Create io_context and socket that will persist for movement loop
    outIoContext = std::make_shared<boost::asio::io_context>();
    outSocket = std::make_shared<Tcp::socket>(*outIoContext);
    
    boost::system::error_code ec;
    
    req::shared::logInfo("TestClient", std::string{"Connecting to zone server at "} + 
        zoneHost + ":" + std::to_string(zonePort) + "...");
    outSocket->connect({ boost::asio::ip::make_address(zoneHost, ec), zonePort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to zone server: " + ec.message());
        return false;
    }
    req::shared::logInfo("TestClient", "Connected to zone server");
    
    // Build and send ZoneAuthRequest
    std::string requestPayload = req::shared::protocol::buildZoneAuthRequestPayload(handoffToken, characterId);
    req::shared::logInfo("TestClient", std::string{"Sending ZoneAuthRequest: handoffToken="} + 
        std::to_string(handoffToken) + ", characterId=" + std::to_string(characterId));
    
    if (!sendMessage(*outSocket, req::shared::MessageType::ZoneAuthRequest, requestPayload)) {
        return false;
    }

    // Receive and parse ZoneAuthResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(*outSocket, header, respBody)) {
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

void TestClient::runMovementTestLoop(std::shared_ptr<boost::asio::io_context> ioContext,
                                     std::shared_ptr<Tcp::socket> zoneSocket,
                                     std::uint64_t localCharacterId) {
    req::shared::logInfo("TestClient", "Zone auth successful. Movement test starting.");
    
    std::cout << "\n=== Movement Test Commands ===\n";
    std::cout << "  w - Move forward\n";
    std::cout << "  s - Move backward\n";
    std::cout << "  a - Strafe left\n";
    std::cout << "  d - Strafe right\n";
    std::cout << "  j - Jump\n";
    std::cout << "  attack <npcId> - Attack an NPC\n";
    std::cout << "  [empty] - Stop moving\n";
    std::cout << "  q - Quit movement test\n";
    std::cout << "==============================\n\n";
    
    std::uint32_t movementSequence = 0;
    bool running = true;
    
    while (running) {
        // Check for any incoming messages from zone server
        req::shared::MessageHeader header;
        std::string msgBody;
        while (tryReceiveMessage(*zoneSocket, header, msgBody)) {
            if (header.type == req::shared::MessageType::PlayerStateSnapshot) {
                req::shared::protocol::PlayerStateSnapshotData snapshot;
                if (req::shared::protocol::parsePlayerStateSnapshotPayload(msgBody, snapshot)) {
                    req::shared::logInfo("TestClient", std::string{"[Snapshot "} + 
                        std::to_string(snapshot.snapshotId) + "] " + 
                        std::to_string(snapshot.players.size()) + " player(s)");
                    
                    // Find our character in the snapshot
                    for (const auto& player : snapshot.players) {
                        if (player.characterId == localCharacterId) {
                            std::cout << "[Snapshot " << snapshot.snapshotId << "] You are at (" 
                                     << player.posX << ", " << player.posY << ", " << player.posZ 
                                     << "), vel=(" << player.velX << ", " << player.velY << ", " << player.velZ 
                                     << "), yaw=" << player.yawDegrees << std::endl;
                            break;
                        }
                    }
                } else {
                    req::shared::logError("TestClient", "Failed to parse PlayerStateSnapshot");
                }
            } else if (header.type == req::shared::MessageType::AttackResult) {
                req::shared::protocol::AttackResultData result;
                if (req::shared::protocol::parseAttackResultPayload(msgBody, result)) {
                    std::cout << "[CLIENT] AttackResult: attackerId=" << result.attackerId
                             << ", targetId=" << result.targetId
                             << ", dmg=" << result.damage
                             << ", hit=" << (result.wasHit ? "YES" : "NO")
                             << ", remainingHp=" << result.remainingHp
                             << ", resultCode=" << result.resultCode
                             << ", msg=\"" << result.message << "\"" << std::endl;
                } else {
                    req::shared::logError("TestClient", "Failed to parse AttackResult");
                }
            } else {
                req::shared::logInfo("TestClient", std::string{"Received unexpected message type: "} + 
                    std::to_string(static_cast<int>(header.type)));
            }
        }
        
        // Prompt for movement command
        std::cout << "\nMovement command: ";
        std::string command;
        std::getline(std::cin, command);
        
        // Trim whitespace
        command.erase(0, command.find_first_not_of(" \t\n\r"));
        command.erase(command.find_last_not_of(" \t\n\r") + 1);
        
        // Check for quit
        if (command == "q" || command == "quit") {
            req::shared::logInfo("TestClient", "User requested quit from movement test");
            running = false;
            break;
        }
        
        // Check for attack command
        if (command.find("attack ") == 0) {
            std::string npcIdStr = command.substr(7); // Skip "attack "
            try {
                std::uint64_t npcId = std::stoull(npcIdStr);
                
                // Build and send AttackRequest
                req::shared::protocol::AttackRequestData attackReq;
                attackReq.attackerCharacterId = localCharacterId;
                attackReq.targetId = npcId;
                attackReq.abilityId = 0;
                attackReq.isBasicAttack = true;
                
                std::string payload = req::shared::protocol::buildAttackRequestPayload(attackReq);
                if (!sendMessage(*zoneSocket, req::shared::MessageType::AttackRequest, payload)) {
                    req::shared::logError("TestClient", "Failed to send AttackRequest");
                } else {
                    req::shared::logInfo("TestClient", std::string{"Sent AttackRequest: target="} +
                        std::to_string(npcId));
                }
            } catch (const std::exception& e) {
                std::cout << "Invalid NPC ID: '" << npcIdStr << "'. Usage: attack <npcId>\n";
            }
            continue;
        }
        
        // Build movement intent from command
        req::shared::protocol::MovementIntentData intent;
        intent.characterId = localCharacterId;
        intent.sequenceNumber = ++movementSequence;
        intent.inputX = 0.0f;
        intent.inputY = 0.0f;
        intent.facingYawDegrees = 0.0f;
        intent.isJumpPressed = false;
        intent.clientTimeMs = getClientTimeMs();
        
        if (command == "w") {
            intent.inputY = 1.0f;      // Forward
            intent.facingYawDegrees = 0.0f;
        } else if (command == "s") {
            intent.inputY = -1.0f;     // Backward
            intent.facingYawDegrees = 180.0f;
        } else if (command == "a") {
            intent.inputX = -1.0f;     // Strafe left
            intent.facingYawDegrees = 270.0f;
        } else if (command == "d") {
            intent.inputX = 1.0f;      // Strafe right
            intent.facingYawDegrees = 90.0f;
        } else if (command == "j") {
            intent.isJumpPressed = true;
            intent.facingYawDegrees = 0.0f;
        } else if (command.empty()) {
            // No input - already set to zeros
        } else {
            std::cout << "Unknown command: '" << command << "'. Use w/a/s/d/j/attack <npcId>/q.\n";
            continue;
        }
        
        // Send MovementIntent
        std::string payload = req::shared::protocol::buildMovementIntentPayload(intent);
        if (!sendMessage(*zoneSocket, req::shared::MessageType::MovementIntent, payload)) {
            req::shared::logError("TestClient", "Failed to send MovementIntent");
            running = false;
            break;
        }
        
        req::shared::logInfo("TestClient", std::string{"Sent MovementIntent: seq="} + 
            std::to_string(intent.sequenceNumber) + ", input=(" + 
            std::to_string(intent.inputX) + "," + std::to_string(intent.inputY) + "), jump=" + 
            (intent.isJumpPressed ? "1" : "0"));
        
        // Give server a moment to respond
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Close zone socket gracefully
    req::shared::logInfo("TestClient", "Closing zone connection");
    boost::system::error_code ec;
    zoneSocket->shutdown(Tcp::socket::shutdown_both, ec);
    zoneSocket->close(ec);
}

} // namespace req::testclient
