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

LoginResponse login(
    const ClientConfig& config,
    const std::string& username,
    const std::string& password,
    req::shared::protocol::LoginMode mode,
    ClientSession& session) {
    
    LoginResponse response;
    
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    
    // Connect to login server
    socket.connect({ boost::asio::ip::make_address(config.loginServerHost, ec), config.loginServerPort }, ec);
    if (ec) {
        response.result = LoginResult::ConnectionFailed;
        response.errorMessage = "Failed to connect to login server: " + ec.message();
        return response;
    }
    
    // Build and send LoginRequest
    std::string requestPayload = req::shared::protocol::buildLoginRequestPayload(
        username, password, config.clientVersion, mode);
    
    if (!sendMessage(socket, req::shared::MessageType::LoginRequest, requestPayload)) {
        response.result = LoginResult::ProtocolError;
        response.errorMessage = "Failed to send LoginRequest";
        return response;
    }
    
    // Receive and parse LoginResponse
    req::shared::MessageHeader header;
    std::string respBody;
    if (!receiveMessage(socket, header, respBody)) {
        response.result = LoginResult::ProtocolError;
        response.errorMessage = "Failed to receive LoginResponse";
        return response;
    }
    
    if (header.type != req::shared::MessageType::LoginResponse) {
        response.result = LoginResult::ProtocolError;
        response.errorMessage = "Unexpected message type from login server";
        return response;
    }
    
    req::shared::protocol::LoginResponseData loginData;
    if (!req::shared::protocol::parseLoginResponsePayload(respBody, loginData)) {
        response.result = LoginResult::ProtocolError;
        response.errorMessage = "Failed to parse LoginResponse";
        return response;
    }
    
    if (!loginData.success) {
        // Map error codes to result enum
        if (loginData.errorCode == "INVALID_PASSWORD" || loginData.errorCode == "ACCOUNT_NOT_FOUND") {
            response.result = LoginResult::InvalidCredentials;
        } else if (loginData.errorCode == "ACCOUNT_BANNED") {
            response.result = LoginResult::AccountBanned;
        } else {
            response.result = LoginResult::ProtocolError;
        }
        response.errorMessage = loginData.errorCode + ": " + loginData.errorMessage;
        return response;
    }
    
    if (loginData.worlds.empty()) {
        response.result = LoginResult::NoWorldsAvailable;
        response.errorMessage = "No worlds available";
        return response;
    }
    
    // Success - populate session
    session.sessionToken = loginData.sessionToken;
    session.isAdmin = loginData.isAdmin;
    
    // Select first world
    const auto& world = loginData.worlds[0];
    session.worldId = world.worldId;
    session.worldHost = world.worldHost;
    session.worldPort = world.worldPort;
    
    response.result = LoginResult::Success;
    response.availableWorlds = loginData.worlds;
    
    return response;
}

} // namespace req::clientcore
