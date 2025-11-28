#include "../include/req/zone/ZoneServer.h"

#include <sstream>
#include <random>
#include <limits>

namespace req::zone {

ZoneServer::ZoneServer(const std::string& address, std::uint16_t port, req::shared::ZoneId zoneId)
    : acceptor_(ioContext_), zoneId_(zoneId) {
    using boost::asio::ip::tcp;
    boost::system::error_code ec;
    tcp::endpoint endpoint(boost::asio::ip::make_address(address, ec), port);
    if (ec) {
        req::shared::logError("zone", std::string{"Invalid listen address: "} + ec.message());
        return;
    }
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) { req::shared::logError("zone", std::string{"acceptor open failed: "} + ec.message()); }
    acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    if (ec) { req::shared::logError("zone", std::string{"acceptor bind failed: "} + ec.message()); }
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) { req::shared::logError("zone", std::string{"acceptor listen failed: "} + ec.message()); }
}

void ZoneServer::run() {
    req::shared::logInfo("zone", "ZoneServer starting (zoneId=" + std::to_string(zoneId_) + ")");
    startAccept();
    ioContext_.run();
}

void ZoneServer::stop() {
    req::shared::logInfo("zone", "ZoneServer shutdown requested");
    ioContext_.stop();
}

void ZoneServer::startAccept() {
    using boost::asio::ip::tcp;
    auto socket = std::make_shared<tcp::socket>(ioContext_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            handleNewConnection(std::move(*socket));
        } else {
            req::shared::logError("zone", std::string{"accept error: "} + ec.message());
        }
        startAccept();
    });
}

void ZoneServer::handleNewConnection(Tcp::socket socket) {
    auto connection = std::make_shared<req::shared::net::Connection>(std::move(socket));
    connections_.push_back(connection);

    connection->setMessageHandler([this](const req::shared::MessageHeader& header,
                                         const req::shared::net::Connection::ByteArray& payload,
                                         std::shared_ptr<req::shared::net::Connection> conn) {
        handleMessage(header, payload, conn);
    });

    req::shared::logInfo("zone", "New client connected");
    connection->start();
}

void ZoneServer::handleMessage(const req::shared::MessageHeader& header,
                               const req::shared::net::Connection::ByteArray& payload,
                               ConnectionPtr connection) {
    std::string body(payload.begin(), payload.end());
    switch (header.type) {
    case req::shared::MessageType::ZoneAuthRequest: {
        // Expected body: handoffToken (decimal). For now, ignore validation.
        if (!body.empty()) {
            try { (void)std::stoull(body); } catch (...) { /* ignore parse errors */ }
        }
        std::string resp = std::string("OK|Welcome to zone ") + std::to_string(zoneId_);
        req::shared::net::Connection::ByteArray respBytes(resp.begin(), resp.end());
        connection->send(req::shared::MessageType::ZoneAuthResponse, respBytes);
        req::shared::logInfo("zone", "ZoneAuthRequest processed");
        break;
    }
    default:
        req::shared::logWarn("zone", "Unsupported message type received");
        break;
    }
}

} // namespace req::zone
