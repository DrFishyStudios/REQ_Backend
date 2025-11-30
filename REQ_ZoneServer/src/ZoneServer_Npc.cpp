#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <fstream>

namespace req::zone {

void ZoneServer::loadNpcsForZone() {
    // Construct NPC config file path
    std::string npcConfigPath = "config/zones/zone_" + std::to_string(zoneId_) + "_npcs.json";
    
    req::shared::logInfo("zone", std::string{"[NPC] Loading NPCs from: "} + npcConfigPath);
    
    // Try to load NPC JSON file
    std::ifstream file(npcConfigPath);
    if (!file.is_open()) {
        req::shared::logInfo("zone", std::string{"[NPC] No NPC file found ("} + npcConfigPath + 
            "), zone will have no NPCs");
        return;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        if (!j.contains("npcs") || !j["npcs"].is_array()) {
            req::shared::logWarn("zone", "[NPC] NPC file does not contain 'npcs' array");
            return;
        }
        
        int loadedCount = 0;
        for (const auto& npcJson : j["npcs"]) {
            req::shared::data::ZoneNpc npc;
            
            // Required fields
            npc.npcId = npcJson.value("npc_id", static_cast<std::uint64_t>(0));
            npc.name = npcJson.value("name", std::string{"Unknown NPC"});
            npc.level = npcJson.value("level", static_cast<std::int32_t>(1));
            npc.maxHp = npcJson.value("max_hp", static_cast<std::int32_t>(100));
            npc.currentHp = npc.maxHp;  // Start at full HP
            npc.isAlive = true;
            
            // Position
            npc.posX = npcJson.value("pos_x", 0.0f);
            npc.posY = npcJson.value("pos_y", 0.0f);
            npc.posZ = npcJson.value("pos_z", 0.0f);
            npc.facingDegrees = npcJson.value("facing_degrees", 0.0f);
            
            // Store spawn point for leashing
            npc.spawnX = npc.posX;
            npc.spawnY = npc.posY;
            npc.spawnZ = npc.posZ;
            
            // Optional fields with defaults
            npc.minDamage = npcJson.value("min_damage", static_cast<std::int32_t>(1));
            npc.maxDamage = npcJson.value("max_damage", static_cast<std::int32_t>(5));
            npc.aggroRadius = npcJson.value("aggro_radius", 10.0f);
            npc.leashRadius = npcJson.value("leash_radius", 50.0f);
            
            // Validate NPC ID
            if (npc.npcId == 0) {
                req::shared::logWarn("zone", "[NPC] Skipping NPC with npc_id=0 (invalid)");
                continue;
            }
            
            // Check for duplicate IDs
            if (npcs_.find(npc.npcId) != npcs_.end()) {
                req::shared::logWarn("zone", std::string{"[NPC] Duplicate npc_id="} + 
                    std::to_string(npc.npcId) + ", skipping");
                continue;
            }
            
            // Add to NPC map
            npcs_[npc.npcId] = npc;
            loadedCount++;
            
            req::shared::logInfo("zone", std::string{"[NPC] Loaded: id="} + std::to_string(npc.npcId) +
                ", name=\"" + npc.name + "\", level=" + std::to_string(npc.level) +
                ", maxHp=" + std::to_string(npc.maxHp) +
                ", pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," +
                std::to_string(npc.posZ) + "), facing=" + std::to_string(npc.facingDegrees));
        }
        
        req::shared::logInfo("zone", std::string{"[NPC] Loaded "} + std::to_string(loadedCount) +
            " NPC(s) for zone " + std::to_string(zoneId_));
        
    } catch (const nlohmann::json::exception& e) {
        req::shared::logError("zone", std::string{"[NPC] JSON parse error: "} + e.what());
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[NPC] Error loading NPCs: "} + e.what());
    }
}

void ZoneServer::updateNpc(req::shared::data::ZoneNpc& npc, float deltaSeconds) {
    // Placeholder for future AI/behavior
    // For now, NPCs are stationary
    (void)npc;
    (void)deltaSeconds;
}

} // namespace req::zone
