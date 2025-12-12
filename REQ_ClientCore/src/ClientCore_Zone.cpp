#include "req/clientcore/ClientCore.h"

#include <array>
#include <chrono>
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
    
    std::uint32_t getClientTimeMs() {
        static auto g_startTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime);
        return static_cast<std::uint32_t>(duration.count());
    }
}

ZoneAuthResponse connectToZone(ClientSession& session) {
    ZoneAuthResponse response;
    
    // Create persistent io_context and socket for zone connection
    session.zoneIoContext = std::make_shared<boost::asio::io_context>();
    session.zoneSocket = std::make_shared<Tcp::socket>(*session.zoneIoContext);
    
    boost::system::error_code ec;
    
    // Connect to zone server
    session.zoneSocket->connect({ 
        boost::asio::ip::make_address(session.zoneHost, ec), 
        session.zonePort 
    }, ec);
    
    if (ec) {
        response.result = ZoneAuthResult::ConnectionFailed;
        response.errorMessage = "Failed to connect to zone server: " + ec.message();
        return response;
    }
    
    // Build and send ZoneAuthRequest
    std::string requestPayload = req::shared::protocol::buildZoneAuthRequestPayload(
        session.handoffToken, session.selectedCharacterId);
    
    if (!sendMessage(*session.zoneSocket, req::shared::MessageType::ZoneAuthRequest, requestPayload)) {
        response.result = ZoneAuthResult::ProtocolError;
        response.errorMessage = "Failed to send ZoneAuthRequest";
        return response;
    }
    
    // Receive and parse ZoneAuthResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(*session.zoneSocket, header, respBody)) {
        response.result = ZoneAuthResult::ProtocolError;
        response.errorMessage = "Failed to receive ZoneAuthResponse";
        return response;
    }
    
    if (header.type != req::shared::MessageType::ZoneAuthResponse) {
        response.result = ZoneAuthResult::ProtocolError;
        response.errorMessage = "Unexpected message type from zone server";
        return response;
    }
    
    req::shared::protocol::ZoneAuthResponseData zoneData;
    if (!req::shared::protocol::parseZoneAuthResponsePayload(respBody, zoneData)) {
        response.result = ZoneAuthResult::ProtocolError;
        response.errorMessage = "Failed to parse ZoneAuthResponse";
        return response;
    }
    
    if (!zoneData.success) {
        // Map error codes
        if (zoneData.errorCode == "INVALID_HANDOFF") {
            response.result = ZoneAuthResult::InvalidHandoff;
        } else if (zoneData.errorCode == "HANDOFF_EXPIRED") {
            response.result = ZoneAuthResult::HandoffExpired;
        } else if (zoneData.errorCode == "WRONG_ZONE") {
            response.result = ZoneAuthResult::WrongZone;
        } else {
            response.result = ZoneAuthResult::ProtocolError;
        }
        response.errorMessage = zoneData.errorCode + ": " + zoneData.errorMessage;
        return response;
    }
    
    // Success
    response.result = ZoneAuthResult::Success;
    response.welcomeMessage = zoneData.welcomeMessage;
    
    return response;
}

bool sendMovementIntent(
    const ClientSession& session,
    float inputX, float inputY,
    float facingYaw, bool jump,
    std::uint32_t sequenceNumber) {
    
    if (!session.zoneSocket || !session.zoneSocket->is_open()) {
        return false;
    }
    
    req::shared::protocol::MovementIntentData intent;
    intent.characterId = session.selectedCharacterId;
    intent.sequenceNumber = sequenceNumber;
    intent.inputX = inputX;
    intent.inputY = inputY;
    intent.facingYawDegrees = facingYaw;
    intent.isJumpPressed = jump;
    intent.clientTimeMs = getClientTimeMs();
    
    std::string payload = req::shared::protocol::buildMovementIntentPayload(intent);
    return sendMessage(*session.zoneSocket, req::shared::MessageType::MovementIntent, payload);
}

bool sendAttackRequest(
    const ClientSession& session,
    std::uint64_t targetId,
    std::uint32_t abilityId,
    bool isBasicAttack) {
    
    if (!session.zoneSocket || !session.zoneSocket->is_open()) {
        return false;
    }
    
    req::shared::protocol::AttackRequestData request;
    request.attackerCharacterId = session.selectedCharacterId;
    request.targetId = targetId;
    request.abilityId = abilityId;
    request.isBasicAttack = isBasicAttack;
    
    std::string payload = req::shared::protocol::buildAttackRequestPayload(request);
    return sendMessage(*session.zoneSocket, req::shared::MessageType::AttackRequest, payload);
}

bool sendDevCommand(
    const ClientSession& session,
    const std::string& command,
    const std::string& param1,
    const std::string& param2) {
    
    if (!session.zoneSocket || !session.zoneSocket->is_open()) {
        return false;
    }
    
    req::shared::protocol::DevCommandData devCmd;
    devCmd.characterId = session.selectedCharacterId;
    devCmd.command = command;
    devCmd.param1 = param1;
    devCmd.param2 = param2;
    
    std::string payload = req::shared::protocol::buildDevCommandPayload(devCmd);
    return sendMessage(*session.zoneSocket, req::shared::MessageType::DevCommand, payload);
}

bool tryReceiveZoneMessage(
    const ClientSession& session,
    ZoneMessage& outMessage) {
    
    if (!session.zoneSocket || !session.zoneSocket->is_open()) {
        return false;
    }
    
    // Set socket to non-blocking mode temporarily
    session.zoneSocket->non_blocking(true);
    
    req::shared::MessageHeader header;
    boost::system::error_code ec;
    std::size_t bytesRead = session.zoneSocket->read_some(
        boost::asio::buffer(&header, sizeof(header)), ec);
    
    if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
        session.zoneSocket->non_blocking(false);
        return false;
    }
    
    if (ec || bytesRead != sizeof(header)) {
        session.zoneSocket->non_blocking(false);
        return false;
    }
    
    // Read body if present
    ByteArray bodyBytes;
    bodyBytes.resize(header.payloadSize);
    if (header.payloadSize > 0) {
        boost::asio::read(*session.zoneSocket, boost::asio::buffer(bodyBytes), ec);
        if (ec) {
            session.zoneSocket->non_blocking(false);
            return false;
        }
    }
    
    session.zoneSocket->non_blocking(false);
    
    // Fill output message
    outMessage.type = header.type;
    outMessage.payload.assign(bodyBytes.begin(), bodyBytes.end());
    
    return true;
}

void disconnectFromZone(ClientSession& session) {
    if (session.zoneSocket && session.zoneSocket->is_open()) {
        boost::system::error_code ec;
        session.zoneSocket->shutdown(Tcp::socket::shutdown_both, ec);
        session.zoneSocket->close(ec);
    }
    
    session.zoneSocket.reset();
    session.zoneIoContext.reset();
}

} // namespace req::clientcore
