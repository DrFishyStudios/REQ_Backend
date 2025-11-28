#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::zone {

ZoneServer::ZoneServer(std::uint32_t worldId,
                       std::uint32_t zoneId,
                       const std::string& zoneName,
                       const std::string& address,
                       std::uint16_t port)
    : acceptor_(ioContext_), worldId_(worldId), zoneId_(zoneId), zoneName_(zoneName), 
      address_(address), port_(port) {
    using boost::asio::ip::tcp;
    boost::system::error_code ec;
    tcp::endpoint endpoint(boost::asio::ip::make_address(address_, ec), port_);
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
    
    // Log ZoneServer construction
    req::shared::logInfo("zone", std::string{"ZoneServer constructed:"});
    req::shared::logInfo("zone", std::string{"  worldId="} + std::to_string(worldId_));
    req::shared::logInfo("zone", std::string{"  zoneId="} + std::to_string(zoneId_));
    req::shared::logInfo("zone", std::string{"  zoneName=\""} + zoneName_ + "\"");
    req::shared::logInfo("zone", std::string{"  address="} + address_);
    req::shared::logInfo("zone", std::string{"  port="} + std::to_string(port_));
}

void ZoneServer::run() {
    req::shared::logInfo("zone", std::string{"ZoneServer starting: worldId="} + 
        std::to_string(worldId_) + ", zoneId=" + std::to_string(zoneId_) + 
        ", zoneName=\"" + zoneName_ + "\", address=" + address_ + 
        ", port=" + std::to_string(port_));
    startAccept();
    req::shared::logInfo("zone", "Entering IO event loop...");
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

    req::shared::logInfo("zone", std::string{"New client connected to zone \""} + zoneName_ + 
        "\" (id=" + std::to_string(zoneId_) + ")");
    connection->start();
}

void ZoneServer::handleMessage(const req::shared::MessageHeader& header,
                               const req::shared::net::Connection::ByteArray& payload,
                               ConnectionPtr connection) {
    // Log protocol version
    req::shared::logInfo("zone", std::string{"Received message: type="} + 
        std::to_string(static_cast<int>(header.type)) + 
        ", protocolVersion=" + std::to_string(header.protocolVersion) +
        ", payloadSize=" + std::to_string(header.payloadSize));

    // Check protocol version
    if (header.protocolVersion != req::shared::CurrentProtocolVersion) {
        req::shared::logWarn("zone", std::string{"Protocol version mismatch: client="} + 
            std::to_string(header.protocolVersion) + ", server=" + std::to_string(req::shared::CurrentProtocolVersion));
    }

    std::string body(payload.begin(), payload.end());

    switch (header.type) {
    case req::shared::MessageType::ZoneAuthRequest: {
        req::shared::HandoffToken handoffToken = 0;
        req::shared::PlayerId characterId = 0;
        
        if (!req::shared::protocol::parseZoneAuthRequestPayload(body, handoffToken, characterId)) {
            req::shared::logError("zone", "Failed to parse ZoneAuthRequest payload");
            auto errPayload = req::shared::protocol::buildZoneAuthResponseErrorPayload("PARSE_ERROR", "Malformed zone auth request");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::ZoneAuthResponse, errBytes);
            return;
        }

        req::shared::logInfo("zone", std::string{"ZoneAuthRequest: handoffToken="} + 
            std::to_string(handoffToken) + ", characterId=" + std::to_string(characterId) +
            " for zone \"" + zoneName_ + "\" (id=" + std::to_string(zoneId_) + ")");

        // TODO: Validate handoffToken with world server or shared handoff service
        // For now, accept any non-zero token
        if (handoffToken == req::shared::InvalidHandoffToken) {
            req::shared::logWarn("zone", "Invalid handoff token");
            auto errPayload = req::shared::protocol::buildZoneAuthResponseErrorPayload("INVALID_HANDOFF", 
                "Handoff token not recognized");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::ZoneAuthResponse, errBytes);
            return;
        }

        // TODO: Load character data, validate character belongs to this player, etc.
        // TODO: Validate this is the correct zone for the handoff token
        
        std::string welcomeMsg = std::string("Welcome to ") + zoneName_ + 
            " (zone " + std::to_string(zoneId_) + " on world " + std::to_string(worldId_) + ")";
        
        auto respPayload = req::shared::protocol::buildZoneAuthResponseOkPayload(welcomeMsg);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        connection->send(req::shared::MessageType::ZoneAuthResponse, respBytes);

        req::shared::logInfo("zone", std::string{"ZoneAuthResponse OK: characterId="} + 
            std::to_string(characterId) + " entered \"" + zoneName_ + "\" (zone " + 
            std::to_string(zoneId_) + " on world " + std::to_string(worldId_) + ")");
        break;
    }
    default:
        req::shared::logWarn("zone", std::string{"Unsupported message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        break;
    }
}

} // namespace req::zone
