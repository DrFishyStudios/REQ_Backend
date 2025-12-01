#include "../include/req/testclient/TestClient.h"

#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"

#include <string>
#include <vector>
#include <iostream>
#include <array>

#include <boost/asio.hpp>

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

bool TestClient::doCharacterList(const std::string& worldHost,
                                 std::uint16_t worldPort,
                                 req::shared::SessionToken sessionToken,
                                 req::shared::WorldId worldId,
                                 std::vector<req::shared::protocol::CharacterListEntry>& outCharacters) {
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
    
    // Build and send CharacterListRequest
    std::string requestPayload = req::shared::protocol::buildCharacterListRequestPayload(sessionToken, worldId);
    req::shared::logInfo("TestClient", std::string{"Sending CharacterListRequest: sessionToken="} + 
        std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId));
    
    if (!sendMessage(socket, req::shared::MessageType::CharacterListRequest, requestPayload)) {
        return false;
    }

    // Receive and parse CharacterListResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::CharacterListResponse) {
        req::shared::logError("TestClient", "Unexpected message type from world server");
        return false;
    }
    
    req::shared::protocol::CharacterListResponseData response;
    if (!req::shared::protocol::parseCharacterListResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse CharacterListResponse");
        return false;
    }
    
    if (!response.success) {
        req::shared::logError("TestClient", std::string{"Character list failed: "} + 
            response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    outCharacters = response.characters;
    return true;
}

bool TestClient::doCharacterCreate(const std::string& worldHost,
                                   std::uint16_t worldPort,
                                   req::shared::SessionToken sessionToken,
                                   req::shared::WorldId worldId,
                                   const std::string& name,
                                   const std::string& race,
                                   const std::string& characterClass,
                                   req::shared::protocol::CharacterListEntry& outCharacter) {
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
    
    // Build and send CharacterCreateRequest
    std::string requestPayload = req::shared::protocol::buildCharacterCreateRequestPayload(
        sessionToken, worldId, name, race, characterClass);
    req::shared::logInfo("TestClient", std::string{"Sending CharacterCreateRequest: name="} + name + 
        ", race=" + race + ", class=" + characterClass);
    
    if (!sendMessage(socket, req::shared::MessageType::CharacterCreateRequest, requestPayload)) {
        return false;
    }

    // Receive and parse CharacterCreateResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::CharacterCreateResponse) {
        req::shared::logError("TestClient", "Unexpected message type from world server");
        return false;
    }
    
    req::shared::protocol::CharacterCreateResponseData response;
    if (!req::shared::protocol::parseCharacterCreateResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse CharacterCreateResponse");
        return false;
    }
    
    if (!response.success) {
        req::shared::logError("TestClient", std::string{"Character creation failed: "} + 
            response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    outCharacter.characterId = response.characterId;
    outCharacter.name = response.name;
    outCharacter.race = response.race;
    outCharacter.characterClass = response.characterClass;
    outCharacter.level = response.level;
    
    return true;
}

bool TestClient::doEnterWorld(const std::string& worldHost,
                              std::uint16_t worldPort,
                              req::shared::SessionToken sessionToken,
                              req::shared::WorldId worldId,
                              std::uint64_t characterId,
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
    
    // Build and send EnterWorldRequest
    std::string requestPayload = req::shared::protocol::buildEnterWorldRequestPayload(
        sessionToken, worldId, characterId);
    req::shared::logInfo("TestClient", std::string{"Sending EnterWorldRequest: sessionToken="} + 
        std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId) + 
        ", characterId=" + std::to_string(characterId));
    
    if (!sendMessage(socket, req::shared::MessageType::EnterWorldRequest, requestPayload)) {
        return false;
    }

    // Receive and parse EnterWorldResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        return false;
    }
    
    if (header.type != req::shared::MessageType::EnterWorldResponse) {
        req::shared::logError("TestClient", "Unexpected message type from world server");
        return false;
    }
    
    req::shared::protocol::EnterWorldResponseData response;
    if (!req::shared::protocol::parseEnterWorldResponsePayload(respBody, response)) {
        req::shared::logError("TestClient", "Failed to parse EnterWorldResponse");
        return false;
    }
    
    if (!response.success) {
        req::shared::logError("TestClient", std::string{"Enter world failed: "} + 
            response.errorCode + " - " + response.errorMessage);
        return false;
    }
    
    outHandoffToken = response.handoffToken;
    outZoneId = response.zoneId;
    outZoneHost = response.zoneHost;
    outZonePort = response.zonePort;
    
    return true;
}

} // namespace req::testclient
