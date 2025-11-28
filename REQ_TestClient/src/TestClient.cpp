#include "../include/req/testclient/TestClient.h"

#include <iostream>
#include <vector>
#include <array>
#include <sstream>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"

namespace req::testclient {

namespace {
    using Tcp = boost::asio::ip::tcp;
    using ByteArray = std::vector<std::uint8_t>;

    bool sendMessage(Tcp::socket& socket,
                     req::shared::MessageType type,
                     const std::string& body) {
        req::shared::MessageHeader header;
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

    std::vector<std::string> splitTokens(const std::string& s) {
        std::vector<std::string> tokens;
        std::size_t start = 0;
        while (true) {
            auto pos = s.find('|', start);
            if (pos == std::string::npos) {
                tokens.emplace_back(s.substr(start));
                break;
            }
            tokens.emplace_back(s.substr(start, pos - start));
            start = pos + 1;
        }
        return tokens;
    }
}

void TestClient::run() {
    std::string worldHost; std::uint16_t worldPort = 0; std::uint64_t sessionToken = 0;
    std::string zoneHost; std::uint16_t zonePort = 0; std::uint64_t handoffToken = 0;

    const std::string username = "test";
    const std::string password = "password";

    if (!doLogin(username, password, worldHost, worldPort, sessionToken)) {
        req::shared::logError("TestClient", "Login failed");
        return;
    }
    req::shared::logInfo("TestClient", "Login succeeded: sessionToken=" + std::to_string(sessionToken) +
        " world=" + worldHost + ":" + std::to_string(worldPort));

    if (!doWorldAuth(worldHost, worldPort, sessionToken, zoneHost, zonePort, handoffToken)) {
        req::shared::logError("TestClient", "World auth failed");
        return;
    }
    req::shared::logInfo("TestClient", "World auth succeeded: handoffToken=" + std::to_string(handoffToken) +
        " zone=" + zoneHost + ":" + std::to_string(zonePort));

    if (!doZoneAuth(zoneHost, zonePort, handoffToken)) {
        req::shared::logError("TestClient", "Zone auth failed");
        return;
    }
    req::shared::logInfo("TestClient", "Zone auth succeeded");
}

bool TestClient::doLogin(const std::string& username,
                         const std::string& password,
                         std::string& outWorldHost,
                         std::uint16_t& outWorldPort,
                         std::uint64_t& outSessionToken) {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    socket.connect({ boost::asio::ip::make_address("127.0.0.1", ec), 7777 }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to login server: " + ec.message());
        return false;
    }
    std::string body = username + '|' + password;
    if (!sendMessage(socket, req::shared::MessageType::LoginRequest, body)) return false;

    req::shared::MessageHeader header; std::string respBody;
    if (!receiveMessage(socket, header, respBody)) return false;
    if (header.type != req::shared::MessageType::LoginResponse) {
        req::shared::logError("TestClient", "Unexpected message type from login server");
        return false;
    }
    auto tokens = splitTokens(respBody);
    if (tokens.size() < 5 || tokens[0] != "OK") {
        req::shared::logError("TestClient", "Invalid login response payload: " + respBody);
        return false;
    }
    try {
        outSessionToken = std::stoull(tokens[1]);
        outWorldHost = tokens[2];
        outWorldPort = static_cast<std::uint16_t>(std::stoul(tokens[3]));
    } catch (...) {
        req::shared::logError("TestClient", "Failed parsing login response fields");
        return false;
    }
    return true;
}

bool TestClient::doWorldAuth(const std::string& worldHost,
                             std::uint16_t worldPort,
                             std::uint64_t sessionToken,
                             std::string& outZoneHost,
                             std::uint16_t& outZonePort,
                             std::uint64_t& outHandoffToken) {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    socket.connect({ boost::asio::ip::make_address(worldHost, ec), worldPort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to world server: " + ec.message());
        return false;
    }
    std::string body = std::to_string(sessionToken);
    if (!sendMessage(socket, req::shared::MessageType::WorldAuthRequest, body)) return false;

    req::shared::MessageHeader header; std::string respBody;
    if (!receiveMessage(socket, header, respBody)) return false;
    if (header.type != req::shared::MessageType::WorldAuthResponse) {
        req::shared::logError("TestClient", "Unexpected message type from world server");
        return false;
    }
    auto tokens = splitTokens(respBody);
    if (tokens.size() < 5 || tokens[0] != "OK") {
        req::shared::logError("TestClient", "Invalid world auth response payload: " + respBody);
        return false;
    }
    try {
        outHandoffToken = std::stoull(tokens[1]);
        outZoneHost = tokens[2];
        outZonePort = static_cast<std::uint16_t>(std::stoul(tokens[3]));
    } catch (...) {
        req::shared::logError("TestClient", "Failed parsing world auth response fields");
        return false;
    }
    return true;
}

bool TestClient::doZoneAuth(const std::string& zoneHost,
                            std::uint16_t zonePort,
                            std::uint64_t handoffToken) {
    boost::asio::io_context io;
    Tcp::socket socket(io);
    boost::system::error_code ec;
    socket.connect({ boost::asio::ip::make_address(zoneHost, ec), zonePort }, ec);
    if (ec) {
        req::shared::logError("TestClient", "Failed to connect to zone server: " + ec.message());
        return false;
    }
    std::string body = std::to_string(handoffToken);
    if (!sendMessage(socket, req::shared::MessageType::ZoneAuthRequest, body)) return false;

    req::shared::MessageHeader header; std::string respBody;
    if (!receiveMessage(socket, header, respBody)) return false;
    if (header.type != req::shared::MessageType::ZoneAuthResponse) {
        req::shared::logError("TestClient", "Unexpected message type from zone server");
        return false;
    }
    auto tokens = splitTokens(respBody);
    if (tokens.size() < 2 || tokens[0] != "OK") {
        req::shared::logError("TestClient", "Invalid zone auth response payload: " + respBody);
        return false;
    }
    req::shared::logInfo("TestClient", "Zone welcome: " + tokens[1]);
    return true;
}

} // namespace req::testclient
