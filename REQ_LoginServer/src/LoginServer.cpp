#include "../include/req/login/LoginServer.h"

#include <random>
#include <sstream>
#include <limits>

#include "../../REQ_Shared/include/req/shared/Logger.h"

namespace req::login {

LoginServer::LoginServer(const req::shared::LoginConfig& config)
    : acceptor_(ioContext_), config_(config) {
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
}

void LoginServer::run() {
    req::shared::logInfo("login", "LoginServer starting on " + config_.address + ":" + std::to_string(config_.port));
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
    std::string body(payload.begin(), payload.end());
    switch (header.type) {
    case req::shared::MessageType::LoginRequest: {
        // Expected format: username|password
        auto sepPos = body.find('|');
        std::string username;
        std::string password;
        if (sepPos != std::string::npos) {
            username = body.substr(0, sepPos);
            password = body.substr(sepPos + 1);
        } else {
            username = body; // whole string
        }
        // TODO: Proper authentication checks
        auto token = generateSessionToken();
        sessions_[token] = username;

        std::ostringstream oss;
        oss << "OK|" << token << '|' << config_.worldHost << '|' << config_.worldPort << "|Welcome to REQ";
        std::string resp = oss.str();
        req::shared::net::Connection::ByteArray respBytes(resp.begin(), resp.end());
        connection->send(req::shared::MessageType::LoginResponse, respBytes);
        req::shared::logInfo("login", "LoginRequest processed for user=" + username);
        break;
    }
    default:
        req::shared::logWarn("login", "Unsupported message type received");
        break;
    }
}

} // namespace req::login
