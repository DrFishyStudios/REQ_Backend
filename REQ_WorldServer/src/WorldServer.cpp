#include "../include/req/world/WorldServer.h"

#include <random>
#include <sstream>
#include <limits>

namespace req::world {

WorldServer::WorldServer(const req::shared::WorldConfig& config)
    : acceptor_(ioContext_), config_(config) {
    using boost::asio::ip::tcp;
    boost::system::error_code ec;
    tcp::endpoint endpoint(boost::asio::ip::make_address(config_.address, ec), config_.port);
    if (ec) {
        req::shared::logError("world", std::string{"Invalid listen address: "} + ec.message());
        return;
    }
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) { req::shared::logError("world", std::string{"acceptor open failed: "} + ec.message()); }
    acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    if (ec) { req::shared::logError("world", std::string{"acceptor bind failed: "} + ec.message()); }
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) { req::shared::logError("world", std::string{"acceptor listen failed: "} + ec.message()); }
}

void WorldServer::run() {
    req::shared::logInfo("world", "WorldServer starting on " + config_.address + ":" + std::to_string(config_.port));
    startAccept();
    ioContext_.run();
}

void WorldServer::stop() {
    req::shared::logInfo("world", "WorldServer shutdown requested");
    ioContext_.stop();
}

void WorldServer::startAccept() {
    using boost::asio::ip::tcp;
    auto socket = std::make_shared<tcp::socket>(ioContext_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            handleNewConnection(std::move(*socket));
        } else {
            req::shared::logError("world", std::string{"accept error: "} + ec.message());
        }
        startAccept();
    });
}

void WorldServer::handleNewConnection(Tcp::socket socket) {
    auto connection = std::make_shared<req::shared::net::Connection>(std::move(socket));
    connections_.push_back(connection);

    connection->setMessageHandler([this](const req::shared::MessageHeader& header,
                                         const req::shared::net::Connection::ByteArray& payload,
                                         std::shared_ptr<req::shared::net::Connection> conn) {
        handleMessage(header, payload, conn);
    });

    req::shared::logInfo("world", "New client connected");
    connection->start();
}

req::shared::HandoffToken WorldServer::generateHandoffToken() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<req::shared::HandoffToken> dist(1, std::numeric_limits<req::shared::HandoffToken>::max());
    req::shared::HandoffToken token = 0;
    do {
        token = dist(rng);
    } while (token == req::shared::InvalidHandoffToken || handoffs_.count(token) != 0);
    return token;
}

void WorldServer::handleMessage(const req::shared::MessageHeader& header,
                                const req::shared::net::Connection::ByteArray& payload,
                                ConnectionPtr connection) {
    std::string body(payload.begin(), payload.end());
    switch (header.type) {
    case req::shared::MessageType::WorldAuthRequest: {
        req::shared::SessionToken sessionToken = 0;
        if (!body.empty()) {
            try {
                sessionToken = static_cast<req::shared::SessionToken>(std::stoull(body));
            } catch (...) {
                sessionToken = 0; // treat parse failure as invalid
            }
        }
        auto handoff = generateHandoffToken();
        handoffs_[handoff] = sessionToken;
        std::ostringstream oss;
        oss << "OK|" << handoff << '|' << config_.zoneHost << '|' << config_.zonePort << "|Entering world...";
        std::string resp = oss.str();
        req::shared::net::Connection::ByteArray respBytes(resp.begin(), resp.end());
        connection->send(req::shared::MessageType::WorldAuthResponse, respBytes);
        req::shared::logInfo("world", "WorldAuthRequest processed (session=" + std::to_string(sessionToken) + ")");
        break;
    }
    default:
        req::shared::logWarn("world", "Unsupported message type received");
        break;
    }
}

} // namespace req::world
