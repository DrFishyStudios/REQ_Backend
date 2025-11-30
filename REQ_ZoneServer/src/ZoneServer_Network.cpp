#include "../include/req/zone/ZoneServer.h"

#include <algorithm>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

namespace req::zone {

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

    connection->setMessageHandler([this, connection](const req::shared::MessageHeader& header,
                                         const req::shared::net::Connection::ByteArray& payload,
                                         std::shared_ptr<req::shared::net::Connection> conn) {
        handleMessage(header, payload, conn);
    });
    
    connection->setDisconnectHandler([this](std::shared_ptr<req::shared::net::Connection> conn) {
        onConnectionClosed(conn);
    });

    req::shared::logInfo("zone", std::string{"New client connected to zone \""} + zoneName_ + 
        "\" (id=" + std::to_string(zoneId_) + "), total connections=" + std::to_string(connections_.size()));
    connection->start();
}

void ZoneServer::onConnectionClosed(ConnectionPtr connection) {
    req::shared::logInfo("zone", "[DISCONNECT] ========== BEGIN DISCONNECT HANDLING ==========");
    req::shared::logInfo("zone", "[DISCONNECT] Connection closed event received");
    
    // Get remote endpoint info for logging (if still available)
    try {
        if (connection && connection->isClosed()) {
            req::shared::logInfo("zone", "[DISCONNECT] Connection is marked as closed");
        }
    } catch (const std::exception& e) {
        req::shared::logWarn("zone", std::string{"[DISCONNECT] Error checking connection state: "} + e.what());
    }
    
    // Find character ID for this connection
    auto it = connectionToCharacterId_.find(connection);
    if (it != connectionToCharacterId_.end()) {
        std::uint64_t characterId = it->second;
        req::shared::logInfo("zone", std::string{"[DISCONNECT] Found ZonePlayer: characterId="} + 
            std::to_string(characterId));
        
        // Check if player exists
        auto playerIt = players_.find(characterId);
        if (playerIt != players_.end()) {
            req::shared::logInfo("zone", std::string{"[DISCONNECT] Player found in players map, accountId="} +
                std::to_string(playerIt->second.accountId) + 
                ", pos=(" + std::to_string(playerIt->second.posX) + "," + 
                std::to_string(playerIt->second.posY) + "," + 
                std::to_string(playerIt->second.posZ) + ")");
        } else {
            req::shared::logWarn("zone", std::string{"[DISCONNECT] CharacterId "} + 
                std::to_string(characterId) + " found in connection map but not in players map (inconsistent state)");
        }
        
        // Remove player (with safe save)
        removePlayer(characterId);
        
        // Remove from connection mapping
        connectionToCharacterId_.erase(it);
        req::shared::logInfo("zone", "[DISCONNECT] Removed from connection-to-character mapping");
    } else {
        req::shared::logInfo("zone", "[DISCONNECT] No ZonePlayer associated with this connection");
        req::shared::logInfo("zone", "[DISCONNECT] Likely disconnected before completing ZoneAuthRequest");
    }
    
    // Remove from connections list
    auto connIt = std::find(connections_.begin(), connections_.end(), connection);
    if (connIt != connections_.end()) {
        connections_.erase(connIt);
        req::shared::logInfo("zone", "[DISCONNECT] Removed from connections list");
    }
    
    req::shared::logInfo("zone", std::string{"[DISCONNECT] Cleanup complete. Active connections="} +
        std::to_string(connections_.size()) + ", active players=" + std::to_string(players_.size()));
    req::shared::logInfo("zone", "[DISCONNECT] ========== END DISCONNECT HANDLING ==========");
}

} // namespace req::zone
