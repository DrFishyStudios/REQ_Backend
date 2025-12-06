#include "../include/req/zone/ZoneServer.h"

#include <cmath>
#include <algorithm>
#include <fstream>
#include <random>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

namespace req::zone {

ZoneServer::ZoneServer(std::uint32_t worldId,
                       std::uint32_t zoneId,
                       const std::string& zoneName,
                       const std::string& address,
                       std::uint16_t port,
                       const req::shared::WorldRules& worldRules,
                       const req::shared::XpTable& xpTable,
                       const std::string& charactersPath)
    : acceptor_(ioContext_), tickTimer_(ioContext_), autosaveTimer_(ioContext_),
      worldId_(worldId), zoneId_(zoneId), zoneName_(zoneName), 
      address_(address), port_(port), worldRules_(worldRules), xpTable_(xpTable),
      characterStore_(charactersPath), accountStore_("data/accounts") {
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
    
    // Initialize zone config with defaults
    zoneConfig_.zoneId = zoneId;
    zoneConfig_.zoneName = zoneName;
    zoneConfig_.safeX = 0.0f;
    zoneConfig_.safeY = 0.0f;
    zoneConfig_.safeZ = 0.0f;
    zoneConfig_.safeYaw = 0.0f;
    zoneConfig_.moveSpeed = 70.0f;
    zoneConfig_.autosaveIntervalSec = 30.0f;
    zoneConfig_.broadcastFullState = true;
    zoneConfig_.interestRadius = 2000.0f;
    zoneConfig_.debugInterest = false;
    
    // Attempt to load zone config from JSON (optional)
    std::string configPath = "config/zones/zone_" + std::to_string(zoneId) + "_config.json";
    try {
        auto loadedConfig = req::shared::loadZoneConfig(configPath);
        
        // Verify zone ID matches
        if (loadedConfig.zoneId != zoneId) {
            req::shared::logWarn("zone", std::string{"Zone config file zone_id ("} + 
                std::to_string(loadedConfig.zoneId) + ") does not match server zone_id (" + 
                std::to_string(zoneId) + "), using defaults");
        } else {
            zoneConfig_ = loadedConfig;
            req::shared::logInfo("zone", std::string{"Loaded zone config from: "} + configPath);
        }
    } catch (const std::exception& e) {
        req::shared::logInfo("zone", std::string{"Zone config not found or invalid ("} + configPath + 
            "), using defaults");
    }
    
    // Log ZoneServer construction
    req::shared::logInfo("zone", std::string{"ZoneServer constructed:"});
    req::shared::logInfo("zone", std::string{"  worldId="} + std::to_string(worldId_));
    req::shared::logInfo("zone", std::string{"  zoneId="} + std::to_string(zoneId_));
    req::shared::logInfo("zone", std::string{"  zoneName=\""} + zoneName_ + "\"");
    req::shared::logInfo("zone", std::string{"  address="} + address_);
    req::shared::logInfo("zone", std::string{"  port="} + std::to_string(port_));
    req::shared::logInfo("zone", std::string{"  charactersPath="} + charactersPath);
    req::shared::logInfo("zone", std::string{"  tickRate=20 Hz"});
    req::shared::logInfo("zone", std::string{"  moveSpeed="} + std::to_string(zoneConfig_.moveSpeed) + " uu/s");
    req::shared::logInfo("zone", std::string{"  broadcastFullState="} + 
        (zoneConfig_.broadcastFullState ? "true" : "false"));
    req::shared::logInfo("zone", std::string{"  interestRadius="} + std::to_string(zoneConfig_.interestRadius));
    
    // Log WorldRules information
    req::shared::logInfo("zone", std::string{"  WorldRules: rulesetId="} + worldRules_.rulesetId);
    req::shared::logInfo("zone", std::string{"    xp.baseRate="} + std::to_string(worldRules_.xp.baseRate));
    req::shared::logInfo("zone", std::string{"    xp.groupBonusPerMember="} + std::to_string(worldRules_.xp.groupBonusPerMember));
    req::shared::logInfo("zone", std::string{"    hotZones="} + std::to_string(worldRules_.hotZones.size()));
    
    // Log XP table information
    if (!xpTable_.entries.empty()) {
        req::shared::logInfo("zone", std::string{"  XpTable: id="} + xpTable_.id + 
            ", maxLevel=" + std::to_string(xpTable_.entries.back().level));
    }
}

void ZoneServer::run() {
    req::shared::logInfo("zone", std::string{"ZoneServer starting: worldId="} + 
        std::to_string(worldId_) + ", zoneId=" + std::to_string(zoneId_) + 
        ", zoneName=\"" + zoneName_ + "\", address=" + address_ + 
        ", port=" + std::to_string(port_));
    
    // Load NPCs for this zone
    loadNpcsForZone();
    
    startAccept();
    
    // Start the simulation tick loop
    scheduleNextTick();
    req::shared::logInfo("zone", "Simulation tick loop started");
    
    // Start position auto-save timer
    scheduleAutosave();
    req::shared::logInfo("zone", std::string{"Position autosave enabled: interval="} +
        std::to_string(zoneConfig_.autosaveIntervalSec) + "s");
    
    req::shared::logInfo("zone", "Entering IO event loop...");
    ioContext_.run();
}

void ZoneServer::stop() {
    req::shared::logInfo("zone", "ZoneServer shutdown requested");
    ioContext_.stop();
}

} // namespace req::zone

