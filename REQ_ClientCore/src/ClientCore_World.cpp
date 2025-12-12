#include "req/clientcore/ClientCore.h"

#include <array>
#include <boost/asio.hpp>

#include "../../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../../REQ_Shared/include/req/shared/Logger.h"

namespace req::clientcore {

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
            req::shared::logError("ClientCore", "Failed to send message: " + ec.message());
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
            req::shared::logError("ClientCore", "Failed to read header: " + ec.message());
            return false;
        }
        
        ByteArray bodyBytes;
        bodyBytes.resize(outHeader.payloadSize);
        if (outHeader.payloadSize > 0) {
            boost::asio::read(socket, boost::asio::buffer(bodyBytes), ec);
            if (ec) {
                req::shared::logError("ClientCore", "Failed to read body: " + ec.message());
                return false;
            }
        }
        outBody.assign(bodyBytes.begin(), bodyBytes.end());
        return true;
    }
}

CharacterListResponse getCharacterList(const ClientSession& session) {
    CharacterListResponse response;
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    // Connect to world server
    socket.connect({ boost::asio::ip::make_address(session.worldHost, ec), session.worldPort }, ec);
    if (ec) {
        response.result = CharacterListResult::ConnectionFailed;
        response.errorMessage = "Failed to connect to world server: " + ec.message();
        return response;
    }
    
    // Build and send CharacterListRequest
    std::string requestPayload = req::shared::protocol::buildCharacterListRequestPayload(
        session.sessionToken, session.worldId);
    
    if (!sendMessage(socket, req::shared::MessageType::CharacterListRequest, requestPayload)) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Failed to send CharacterListRequest";
        return response;
    }
    
    // Receive and parse CharacterListResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Failed to receive CharacterListResponse";
        return response;
    }
    
    if (header.type != req::shared::MessageType::CharacterListResponse) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Unexpected message type from world server";
        return response;
    }
    
    req::shared::protocol::CharacterListResponseData charListData;
    if (!req::shared::protocol::parseCharacterListResponsePayload(respBody, charListData)) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Failed to parse CharacterListResponse";
        return response;
    }
    
    if (!charListData.success) {
        // Map error codes
        if (charListData.errorCode == "INVALID_SESSION") {
            response.result = CharacterListResult::InvalidSession;
        } else {
            response.result = CharacterListResult::ProtocolError;
        }
        response.errorMessage = charListData.errorCode + ": " + charListData.errorMessage;
        return response;
    }
    
    // Success
    response.result = CharacterListResult::Success;
    response.characters = charListData.characters;
    
    return response;
}

CharacterCreateResponse createCharacter(
    const ClientSession& session,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass) {
    
    CharacterCreateResponse response;
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    // Connect to world server
    socket.connect({ boost::asio::ip::make_address(session.worldHost, ec), session.worldPort }, ec);
    if (ec) {
        response.result = CharacterListResult::ConnectionFailed;
        response.errorMessage = "Failed to connect to world server: " + ec.message();
        return response;
    }
    
    // Build and send CharacterCreateRequest
    std::string requestPayload = req::shared::protocol::buildCharacterCreateRequestPayload(
        session.sessionToken, session.worldId, name, race, characterClass);
    
    if (!sendMessage(socket, req::shared::MessageType::CharacterCreateRequest, requestPayload)) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Failed to send CharacterCreateRequest";
        return response;
    }
    
    // Receive and parse CharacterCreateResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Failed to receive CharacterCreateResponse";
        return response;
    }
    
    if (header.type != req::shared::MessageType::CharacterCreateResponse) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Unexpected message type from world server";
        return response;
    }
    
    req::shared::protocol::CharacterCreateResponseData createData;
    if (!req::shared::protocol::parseCharacterCreateResponsePayload(respBody, createData)) {
        response.result = CharacterListResult::ProtocolError;
        response.errorMessage = "Failed to parse CharacterCreateResponse";
        return response;
    }
    
    if (!createData.success) {
        // Map error codes
        if (createData.errorCode == "INVALID_SESSION") {
            response.result = CharacterListResult::InvalidSession;
        } else {
            response.result = CharacterListResult::ProtocolError;
        }
        response.errorMessage = createData.errorCode + ": " + createData.errorMessage;
        return response;
    }
    
    // Success - build character entry
    response.result = CharacterListResult::Success;
    response.newCharacter.characterId = createData.characterId;
    response.newCharacter.name = createData.name;
    response.newCharacter.race = createData.race;
    response.newCharacter.characterClass = createData.characterClass;
    response.newCharacter.level = createData.level;
    
    return response;
}

EnterWorldResponse enterWorld(
    ClientSession& session,
    std::uint64_t characterId) {
    
    EnterWorldResponse response;
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    // Connect to world server
    socket.connect({ boost::asio::ip::make_address(session.worldHost, ec), session.worldPort }, ec);
    if (ec) {
        response.result = EnterWorldResult::ConnectionFailed;
        response.errorMessage = "Failed to connect to world server: " + ec.message();
        return response;
    }
    
    // Build and send EnterWorldRequest
    std::string requestPayload = req::shared::protocol::buildEnterWorldRequestPayload(
        session.sessionToken, session.worldId, characterId);
    
    if (!sendMessage(socket, req::shared::MessageType::EnterWorldRequest, requestPayload)) {
        response.result = EnterWorldResult::ProtocolError;
        response.errorMessage = "Failed to send EnterWorldRequest";
        return response;
    }
    
    // Receive and parse EnterWorldResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        response.result = EnterWorldResult::ProtocolError;
        response.errorMessage = "Failed to receive EnterWorldResponse";
        return response;
    }
    
    if (header.type != req::shared::MessageType::EnterWorldResponse) {
        response.result = EnterWorldResult::ProtocolError;
        response.errorMessage = "Unexpected message type from world server";
        return response;
    }
    
    req::shared::protocol::EnterWorldResponseData enterData;
    if (!req::shared::protocol::parseEnterWorldResponsePayload(respBody, enterData)) {
        response.result = EnterWorldResult::ProtocolError;
        response.errorMessage = "Failed to parse EnterWorldResponse";
        return response;
    }
    
    if (!enterData.success) {
        // Map error codes
        if (enterData.errorCode == "INVALID_SESSION") {
            response.result = EnterWorldResult::InvalidSession;
        } else if (enterData.errorCode == "INVALID_CHARACTER") {
            response.result = EnterWorldResult::InvalidCharacter;
        } else {
            response.result = EnterWorldResult::ProtocolError;
        }
        response.errorMessage = enterData.errorCode + ": " + enterData.errorMessage;
        return response;
    }
    
    // Success - populate session with zone handoff info
    session.handoffToken = enterData.handoffToken;
    session.zoneId = enterData.zoneId;
    session.zoneHost = enterData.zoneHost;
    session.zonePort = enterData.zonePort;
    session.selectedCharacterId = characterId;
    
    response.result = EnterWorldResult::Success;
    
    return response;
}

} // namespace req::clientcore
