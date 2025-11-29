// Negative test mode implementation for TestClient
// Tests error handling by sending intentionally invalid requests

#include "../include/req/testclient/TestClient.h"

#include <iostream>
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
    
    bool sendRawMessage(Tcp::socket& socket,
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

void TestClient::runNegativeTests() {
    req::shared::logInfo("TestClient", "=== NEGATIVE TEST MODE ===");
    req::shared::logInfo("TestClient", "Testing error handling by sending invalid requests");
    req::shared::logInfo("TestClient", "");
    
    // Default zone server endpoint for testing
    std::string zoneHost = "127.0.0.1";
    std::uint16_t zonePort = 7000;
    
    std::cout << "\n--- Negative Tests Configuration ---\n";
    std::cout << "Default zone endpoint: " << zoneHost << ":" << zonePort << "\n";
    std::cout << "Press Enter to continue with default, or type custom endpoint (host:port): ";
    
    std::string input;
    std::getline(std::cin, input);
    
    if (!input.empty()) {
        auto colonPos = input.find(':');
        if (colonPos != std::string::npos) {
            zoneHost = input.substr(0, colonPos);
            try {
                zonePort = static_cast<std::uint16_t>(std::stoul(input.substr(colonPos + 1)));
            } catch (...) {
                req::shared::logWarn("TestClient", "Invalid port, using default 7000");
                zonePort = 7000;
            }
        }
    }
    
    req::shared::logInfo("TestClient", std::string{"Using zone endpoint: "} + zoneHost + ":" + std::to_string(zonePort));
    req::shared::logInfo("TestClient", "");
    
    // Run negative tests
    bool allPassed = true;
    
    req::shared::logInfo("TestClient", "--- Test 1: Invalid HandoffToken (0) ---");
    if (testInvalidZoneAuth(zoneHost, zonePort)) {
        req::shared::logInfo("TestClient", "? Test 1 PASSED: Server correctly rejected invalid handoffToken");
    } else {
        req::shared::logError("TestClient", "? Test 1 FAILED: Server did not handle invalid handoffToken correctly");
        allPassed = false;
    }
    req::shared::logInfo("TestClient", "");
    
    req::shared::logInfo("TestClient", "--- Test 2: Malformed ZoneAuthRequest payload ---");
    if (testMalformedZoneAuth(zoneHost, zonePort)) {
        req::shared::logInfo("TestClient", "? Test 2 PASSED: Server correctly rejected malformed payload");
    } else {
        req::shared::logError("TestClient", "? Test 2 FAILED: Server did not handle malformed payload correctly");
        allPassed = false;
    }
    req::shared::logInfo("TestClient", "");
    
    // Summary
    req::shared::logInfo("TestClient", "=== NEGATIVE TEST SUMMARY ===");
    if (allPassed) {
        req::shared::logInfo("TestClient", "? ALL TESTS PASSED");
        req::shared::logInfo("TestClient", "Server error handling is working correctly");
    } else {
        req::shared::logError("TestClient", "? SOME TESTS FAILED");
        req::shared::logError("TestClient", "Review server logs and error handling");
    }
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
}

bool TestClient::testInvalidZoneAuth(const std::string& zoneHost, std::uint16_t zonePort) {
    req::shared::logInfo("TestClient", "Sending ZoneAuthRequest with handoffToken=0 (InvalidHandoffToken)");
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    // Connect to zone server
    req::shared::logInfo("TestClient", std::string{"Connecting to zone server at "} + 
        zoneHost + ":" + std::to_string(zonePort) + "...");
    socket.connect({ boost::asio::ip::make_address(zoneHost, ec), zonePort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to zone server: " + ec.message());
        return false;
    }
    req::shared::logInfo("TestClient", "Connected");
    
    // Build ZoneAuthRequest with invalid handoffToken (0)
    req::shared::HandoffToken invalidHandoff = 0; // InvalidHandoffToken
    req::shared::PlayerId characterId = 12345;    // Any character ID
    
    std::string requestPayload = req::shared::protocol::buildZoneAuthRequestPayload(invalidHandoff, characterId);
    req::shared::logInfo("TestClient", std::string{"Sending: handoffToken="} + 
        std::to_string(invalidHandoff) + ", characterId=" + std::to_string(characterId));
    req::shared::logInfo("TestClient", std::string{"Payload: '"} + requestPayload + "'");
    
    if (!sendRawMessage(socket, req::shared::MessageType::ZoneAuthRequest, requestPayload)) {
        return false;
    }
    
    // Receive response
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        req::shared::logError("TestClient", "No response received - SILENT FAILURE");
        return false;
    }
    
    // Verify response
    if (header.type != req::shared::MessageType::ZoneAuthResponse) {
        req::shared::logError("TestClient", std::string{"Unexpected message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        return false;
    }
    
    req::shared::logInfo("TestClient", std::string{"Received ZoneAuthResponse, payload: '"} + respBody + "'");
    
    // Parse response
    req::shared::protocol::ZoneAuthResponseData response;
    if (!req::shared::protocol::parseZoneAuthResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse ZoneAuthResponse");
        return false;
    }
    
    // Verify it's an error response
    if (response.success) {
        req::shared::logError("TestClient", "Server accepted invalid handoffToken - INCORRECT BEHAVIOR");
        return false;
    }
    
    // Verify error code
    req::shared::logInfo("TestClient", std::string{"Error response received: errorCode='"} + 
        response.errorCode + "', errorMessage='" + response.errorMessage + "'");
    
    if (response.errorCode != "INVALID_HANDOFF") {
        req::shared::logWarn("TestClient", std::string{"Expected errorCode='INVALID_HANDOFF', got '"} + 
            response.errorCode + "'");
        // Not a failure, just different error code
    }
    
    socket.close(ec);
    return true;
}

bool TestClient::testMalformedZoneAuth(const std::string& zoneHost, std::uint16_t zonePort) {
    req::shared::logInfo("TestClient", "Sending ZoneAuthRequest with malformed payload");
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    // Connect to zone server
    req::shared::logInfo("TestClient", std::string{"Connecting to zone server at "} + 
        zoneHost + ":" + std::to_string(zonePort) + "...");
    socket.connect({ boost::asio::ip::make_address(zoneHost, ec), zonePort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to zone server: " + ec.message());
        return false;
    }
    req::shared::logInfo("TestClient", "Connected");
    
    // Build malformed payload (missing characterId field)
    std::string malformedPayload = "12345"; // Only handoffToken, no separator or characterId
    
    req::shared::logInfo("TestClient", std::string{"Sending malformed payload: '"} + malformedPayload + "'");
    
    if (!sendRawMessage(socket, req::shared::MessageType::ZoneAuthRequest, malformedPayload)) {
        return false;
    }
    
    // Receive response
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        req::shared::logError("TestClient", "No response received - SILENT FAILURE");
        return false;
    }
    
    // Verify response
    if (header.type != req::shared::MessageType::ZoneAuthResponse) {
        req::shared::logError("TestClient", std::string{"Unexpected message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        return false;
    }
    
    req::shared::logInfo("TestClient", std::string{"Received ZoneAuthResponse, payload: '"} + respBody + "'");
    
    // Parse response
    req::shared::protocol::ZoneAuthResponseData response;
    if (!req::shared::protocol::parseZoneAuthResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse ZoneAuthResponse");
        return false;
    }
    
    // Verify it's an error response
    if (response.success) {
        req::shared::logError("TestClient", "Server accepted malformed payload - INCORRECT BEHAVIOR");
        return false;
    }
    
    // Verify error code
    req::shared::logInfo("TestClient", std::string{"Error response received: errorCode='"} + 
        response.errorCode + "', errorMessage='" + response.errorMessage + "'");
    
    if (response.errorCode != "PARSE_ERROR") {
        req::shared::logWarn("TestClient", std::string{"Expected errorCode='PARSE_ERROR', got '"} + 
            response.errorCode + "'");
        // Not a failure, just different error code
    }
    
    socket.close(ec);
    return true;
}

} // namespace req::testclient
