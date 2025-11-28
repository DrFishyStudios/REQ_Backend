#include "../include/req/login/LoginServer.h"

#include <random>
#include <limits>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::login {

LoginServer::LoginServer(const req::shared::LoginConfig& config,
                         const req::shared::WorldListConfig& worldList)
    : acceptor_(ioContext_), config_(config), worlds_(worldList.worlds) {
    using boost::asio::ip::tcp;
    boost::system::error_code ec;
    tcp::endpoint endpoint(boost::asio::ip::make_address(config_.address, ec), config_.port);
    if (ec) {
        req::shared::logError("login", std::string{"Invalid listen address: "} + ec.message());
        return;
    }
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) { req::shared::logError("login", std::string{"acceptor open failed: "} + ec.message()); }
    acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    if (ec) { req::shared::logError("login", std::string{"acceptor bind failed: "} + ec.message()); }
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) { req::shared::logError("login", std::string{"acceptor listen failed: "} + ec.message()); }
    
    // Log world list on construction
    req::shared::logInfo("login", std::string{"LoginServer initialized with "} + 
        std::to_string(worlds_.size()) + " world(s)");
}

void LoginServer::run() {
    req::shared::logInfo("login", "LoginServer starting on " + config_.address + ":" + std::to_string(config_.port));
    if (!config_.motd.empty()) {
        req::shared::logInfo("login", "MOTD: " + config_.motd);
    }
    startAccept();
    ioContext_.run();
}

void LoginServer::stop() {
    req::shared::logInfo("login", "LoginServer shutdown requested");
    ioContext_.stop();
}

void LoginServer::startAccept() {
    using boost::asio::ip::tcp;
    auto socket = std::make_shared<tcp::socket>(ioContext_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            handleNewConnection(std::move(*socket));
        } else {
            req::shared::logError("login", std::string{"accept error: "} + ec.message());
        }
        startAccept();
    });
}

void LoginServer::handleNewConnection(Tcp::socket socket) {
    auto connection = std::make_shared<req::shared::net::Connection>(std::move(socket));
    connections_.push_back(connection);

    connection->setMessageHandler([this](const req::shared::MessageHeader& header,
                                         const req::shared::net::Connection::ByteArray& payload,
                                         std::shared_ptr<req::shared::net::Connection> conn) {
        handleMessage(header, payload, conn);
    });

    req::shared::logInfo("login", "New client connected");
    connection->start();
}

req::shared::SessionToken LoginServer::generateSessionToken() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<req::shared::SessionToken> dist(1, std::numeric_limits<req::shared::SessionToken>::max());
    req::shared::SessionToken token = 0;
    do {
        token = dist(rng);
    } while (token == req::shared::InvalidSessionToken || sessions_.count(token) != 0);
    return token;
}

void LoginServer::handleMessage(const req::shared::MessageHeader& header,
                                const req::shared::net::Connection::ByteArray& payload,
                                ConnectionPtr connection) {
    // Log protocol version
    req::shared::logInfo("login", std::string{"Received message: type="} + 
        std::to_string(static_cast<int>(header.type)) + 
        ", protocolVersion=" + std::to_string(header.protocolVersion) +
        ", payloadSize=" + std::to_string(header.payloadSize));

    // Check protocol version
    if (header.protocolVersion != req::shared::CurrentProtocolVersion) {
        req::shared::logWarn("login", std::string{"Protocol version mismatch: client="} + 
            std::to_string(header.protocolVersion) + ", server=" + std::to_string(req::shared::CurrentProtocolVersion));
        // TODO: In future, enforce strict version matching
    }

    std::string body(payload.begin(), payload.end());

    switch (header.type) {
    case req::shared::MessageType::LoginRequest: {
        std::string username, password, clientVersion;
        if (!req::shared::protocol::parseLoginRequestPayload(body, username, password, clientVersion)) {
            req::shared::logError("login", "Failed to parse LoginRequest payload");
            auto errPayload = req::shared::protocol::buildLoginResponseErrorPayload("PARSE_ERROR", "Malformed login request");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::LoginResponse, errBytes);
            return;
        }

        req::shared::logInfo("login", std::string{"LoginRequest: username="} + username + 
            ", clientVersion=" + clientVersion);

        // TODO: Proper authentication checks (database lookup, password hash verification)
        // For now, accept any non-empty username
        if (username.empty()) {
            req::shared::logWarn("login", "Login rejected: empty username");
            auto errPayload = req::shared::protocol::buildLoginResponseErrorPayload("AUTH_FAILED", "Username cannot be empty");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::LoginResponse, errBytes);
            return;
        }

        auto token = generateSessionToken();
        sessions_[token] = username;

        // Build world list from worlds_ (loaded from worlds.json)
        std::vector<req::shared::protocol::WorldListEntry> worldEntries;
        for (const auto& world : worlds_) {
            req::shared::protocol::WorldListEntry entry;
            entry.worldId = world.worldId;
            entry.worldName = world.worldName;
            entry.worldHost = world.host;
            entry.worldPort = world.port;
            entry.rulesetId = world.rulesetId;
            worldEntries.push_back(entry);
        }

        auto respPayload = req::shared::protocol::buildLoginResponseOkPayload(token, worldEntries);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        connection->send(req::shared::MessageType::LoginResponse, respBytes);

        req::shared::logInfo("login", std::string{"LoginResponse OK: username="} + username + 
            ", sessionToken=" + std::to_string(token) + 
            ", worldCount=" + std::to_string(worldEntries.size()));
        
        // Log each world for debugging
        for (const auto& world : worldEntries) {
            req::shared::logInfo("login", std::string{"  World: id="} + std::to_string(world.worldId) + 
                ", name=" + world.worldName + 
                ", endpoint=" + world.worldHost + ":" + std::to_string(world.worldPort) +
                ", ruleset=" + world.rulesetId);
        }
        break;
    }
    default:
        req::shared::logWarn("login", std::string{"Unsupported message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        break;
    }
}

} // namespace req::login
