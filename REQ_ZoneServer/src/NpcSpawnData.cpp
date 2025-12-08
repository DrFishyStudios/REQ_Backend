#include "../include/req/zone/NpcSpawnData.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <fstream>
#include <unordered_set>

using json = nlohmann::json;
using req::shared::logInfo;
using req::shared::logWarn;
using req::shared::logError;

namespace req::zone {

namespace {

/**
 * Helper: Parse NPC template from JSON object
 */
bool parseNpcTemplate(const json& j, NpcTemplateData& out) {
    try {
        out.npcId = j.value("npc_id", 0);
        out.name = j.value("name", std::string{});
        out.level = j.value("level", 1);
        out.archetype = j.value("archetype", std::string{"melee_trash"});
        
        // Combat stats
        out.hp = j.value("hp", 100);
        out.ac = j.value("ac", 10);
        out.minDamage = j.value("min_damage", 1);
        out.maxDamage = j.value("max_damage", 5);
        
        // References
        out.factionId = j.value("faction_id", 0);
        out.lootTableId = j.value("loot_table_id", 0);
        
        // visual_id can be int or string
        if (j.contains("visual_id")) {
            if (j["visual_id"].is_string()) {
                out.visualId = j["visual_id"].get<std::string>();
            } else if (j["visual_id"].is_number()) {
                out.visualId = std::to_string(j["visual_id"].get<int>());
            }
        }
        
        // Behavior flags
        out.isSocial = j.value("is_social", false);
        out.canFlee = j.value("can_flee", false);
        out.isRoamer = j.value("is_roamer", false);
        
        // AI parameters
        out.aggroRadius = j.value("aggro_radius", 10.0f);
        out.assistRadius = j.value("assist_radius", 15.0f);
        
        return true;
    } catch (const json::exception& e) {
        logError("NpcDataRepository", std::string{"Failed to parse NPC template: "} + e.what());
        return false;
    }
}

/**
 * Helper: Parse spawn point from JSON object
 */
bool parseSpawnPoint(const json& j, NpcSpawnPointData& out) {
    try {
        out.spawnId = j.value("spawn_id", 0);
        out.npcId = j.value("npc_id", 0);
        
        // Position (required)
        if (j.contains("position") && j["position"].is_object()) {
            const auto& pos = j["position"];
            out.posX = pos.value("x", 0.0f);
            out.posY = pos.value("y", 0.0f);
            out.posZ = pos.value("z", 0.0f);
        } else {
            // Fallback: try direct x/y/z fields
            out.posX = j.value("x", 0.0f);
            out.posY = j.value("y", 0.0f);
            out.posZ = j.value("z", 0.0f);
        }
        
        out.heading = j.value("heading", 0.0f);
        
        // Respawn timing
        out.respawnSeconds = j.value("respawn_seconds", 120);
        out.respawnVarianceSeconds = j.value("respawn_variance_seconds", 0);
        
        // Optional grouping
        out.spawnGroup = j.value("spawn_group", std::string{});
        
        return true;
    } catch (const json::exception& e) {
        logError("NpcDataRepository", std::string{"Failed to parse spawn point: "} + e.what());
        return false;
    }
}

} // anonymous namespace

bool NpcDataRepository::LoadNpcTemplates(const std::string& path) {
    logInfo("NpcDataRepository", std::string{"Loading NPC templates from: "} + path);
    
    std::ifstream file(path);
    if (!file.is_open()) {
        logError("NpcDataRepository", std::string{"Failed to open NPC templates file: "} + path);
        return false;
    }
    
    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        logError("NpcDataRepository", std::string{"Failed to parse JSON from "} + path + ": " + e.what());
        return false;
    }
    
    // Expect a "templates" array
    if (!j.contains("templates") || !j["templates"].is_array()) {
        logError("NpcDataRepository", "NPC templates file does not contain 'templates' array");
        return false;
    }
    
    const auto& templatesArray = j["templates"];
    if (templatesArray.empty()) {
        logWarn("NpcDataRepository", "NPC templates file contains empty 'templates' array");
        return true;  // Not an error, just no templates
    }
    
    std::unordered_set<std::int32_t> seenIds;
    int loadedCount = 0;
    int skippedCount = 0;
    
    for (const auto& templateJson : templatesArray) {
        NpcTemplateData tmpl;
        
        if (!parseNpcTemplate(templateJson, tmpl)) {
            skippedCount++;
            continue;
        }
        
        // Validate npc_id
        if (tmpl.npcId == 0) {
            logWarn("NpcDataRepository", "Skipping NPC template with npc_id=0 (invalid)");
            skippedCount++;
            continue;
        }
        
        // Check for duplicates
        if (seenIds.find(tmpl.npcId) != seenIds.end()) {
            logWarn("NpcDataRepository", std::string{"Duplicate npc_id="} + std::to_string(tmpl.npcId) + ", skipping");
            skippedCount++;
            continue;
        }
        
        // Validate data
        if (tmpl.name.empty()) {
            logWarn("NpcDataRepository", std::string{"NPC template "} + std::to_string(tmpl.npcId) + " has empty name, skipping");
            skippedCount++;
            continue;
        }
        
        if (tmpl.level < 1) {
            logWarn("NpcDataRepository", std::string{"NPC template "} + std::to_string(tmpl.npcId) + 
                    " has invalid level " + std::to_string(tmpl.level) + ", using 1");
            tmpl.level = 1;
        }
        
        if (tmpl.hp <= 0) {
            logWarn("NpcDataRepository", std::string{"NPC template "} + std::to_string(tmpl.npcId) + 
                    " has invalid HP " + std::to_string(tmpl.hp) + ", using 100");
            tmpl.hp = 100;
        }
        
        if (tmpl.minDamage > tmpl.maxDamage) {
            logWarn("NpcDataRepository", std::string{"NPC template "} + std::to_string(tmpl.npcId) + 
                    " has min_damage > max_damage, swapping");
            std::swap(tmpl.minDamage, tmpl.maxDamage);
        }
        
        // Add to repository
        templates_[tmpl.npcId] = tmpl;
        seenIds.insert(tmpl.npcId);
        loadedCount++;
        
        logInfo("NpcDataRepository", std::string{"  Loaded NPC template: id="} + std::to_string(tmpl.npcId) + 
                ", name=\"" + tmpl.name + "\"" +
                ", level=" + std::to_string(tmpl.level) +
                ", hp=" + std::to_string(tmpl.hp) +
                ", archetype=" + tmpl.archetype);
    }
    
    logInfo("NpcDataRepository", std::string{"Loaded "} + std::to_string(loadedCount) + 
            " NPC template(s)" + (skippedCount > 0 ? " (" + std::to_string(skippedCount) + " skipped)" : ""));
    
    return loadedCount > 0;
}

bool NpcDataRepository::LoadZoneSpawns(const std::string& path) {
    logInfo("NpcDataRepository", std::string{"Loading zone spawns from: "} + path);
    
    std::ifstream file(path);
    if (!file.is_open()) {
        logWarn("NpcDataRepository", std::string{"Zone spawn file not found: "} + path + " (zone will have no NPCs)");
        return true;  // Not a fatal error - zone can run without NPCs
    }
    
    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        logError("NpcDataRepository", std::string{"Failed to parse JSON from "} + path + ": " + e.what());
        return false;
    }
    
    // Read zone_id
    zoneId_ = j.value("zone_id", 0u);
    if (zoneId_ == 0) {
        logWarn("NpcDataRepository", "Spawn file has zone_id=0 or missing");
    }
    
    // Expect a "spawns" array
    if (!j.contains("spawns") || !j["spawns"].is_array()) {
        logWarn("NpcDataRepository", "Spawn file does not contain 'spawns' array");
        return true;  // Not fatal, just no spawns
    }
    
    const auto& spawnsArray = j["spawns"];
    if (spawnsArray.empty()) {
        logWarn("NpcDataRepository", "Spawn file contains empty 'spawns' array");
        return true;
    }
    
    std::unordered_set<std::int32_t> seenIds;
    int loadedCount = 0;
    int skippedCount = 0;
    
    for (const auto& spawnJson : spawnsArray) {
        NpcSpawnPointData spawn;
        
        if (!parseSpawnPoint(spawnJson, spawn)) {
            skippedCount++;
            continue;
        }
        
        // Validate spawn_id
        if (spawn.spawnId == 0) {
            logWarn("NpcDataRepository", "Skipping spawn with spawn_id=0 (invalid)");
            skippedCount++;
            continue;
        }
        
        // Check for duplicates
        if (seenIds.find(spawn.spawnId) != seenIds.end()) {
            logWarn("NpcDataRepository", std::string{"Duplicate spawn_id="} + std::to_string(spawn.spawnId) + ", skipping");
            skippedCount++;
            continue;
        }
        
        // Validate npc_id reference
        if (spawn.npcId == 0) {
            logWarn("NpcDataRepository", std::string{"Spawn "} + std::to_string(spawn.spawnId) + " has npc_id=0, skipping");
            skippedCount++;
            continue;
        }
        
        if (templates_.find(spawn.npcId) == templates_.end()) {
            logWarn("NpcDataRepository", std::string{"Spawn "} + std::to_string(spawn.spawnId) + 
                    " references non-existent npc_id=" + std::to_string(spawn.npcId) + ", skipping");
            skippedCount++;
            continue;
        }
        
        // Validate respawn timing
        if (spawn.respawnSeconds < 0) {
            logWarn("NpcDataRepository", std::string{"Spawn "} + std::to_string(spawn.spawnId) + 
                    " has negative respawn_seconds, using 120");
            spawn.respawnSeconds = 120;
        }
        
        if (spawn.respawnVarianceSeconds < 0) {
            logWarn("NpcDataRepository", std::string{"Spawn "} + std::to_string(spawn.spawnId) + 
                    " has negative respawn_variance_seconds, using 0");
            spawn.respawnVarianceSeconds = 0;
        }
        
        // Add to repository
        spawnPoints_.push_back(spawn);
        seenIds.insert(spawn.spawnId);
        loadedCount++;
        
        const NpcTemplateData* tmpl = GetTemplate(spawn.npcId);
        std::string npcName = tmpl ? tmpl->name : "Unknown";
        
        logInfo("NpcDataRepository", std::string{"  Loaded spawn: id="} + std::to_string(spawn.spawnId) + 
                ", npc_id=" + std::to_string(spawn.npcId) +
                " (" + npcName + ")" +
                ", pos=(" + std::to_string(spawn.posX) + "," + std::to_string(spawn.posY) + "," + std::to_string(spawn.posZ) + ")" +
                ", respawn=" + std::to_string(spawn.respawnSeconds) + "s" +
                (spawn.spawnGroup.empty() ? "" : ", group=" + spawn.spawnGroup));
    }
    
    logInfo("NpcDataRepository", std::string{"Loaded "} + std::to_string(loadedCount) + 
            " spawn point(s) for zone " + std::to_string(zoneId_) +
            (skippedCount > 0 ? " (" + std::to_string(skippedCount) + " skipped)" : ""));
    
    return loadedCount > 0;
}

const NpcTemplateData* NpcDataRepository::GetTemplate(std::int32_t npcId) const {
    auto it = templates_.find(npcId);
    if (it != templates_.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::vector<NpcSpawnPointData>& NpcDataRepository::GetZoneSpawns() const {
    return spawnPoints_;
}

const NpcSpawnPointData* NpcDataRepository::GetSpawnPoint(std::int32_t spawnId) const {
    for (const auto& spawn : spawnPoints_) {
        if (spawn.spawnId == spawnId) {
            return &spawn;
        }
    }
    return nullptr;
}

const std::unordered_map<std::int32_t, NpcTemplateData>& NpcDataRepository::GetAllTemplates() const {
    return templates_;
}

std::size_t NpcDataRepository::GetTemplateCount() const {
    return templates_.size();
}

std::size_t NpcDataRepository::GetSpawnCount() const {
    return spawnPoints_.size();
}

void NpcDataRepository::Clear() {
    templates_.clear();
    spawnPoints_.clear();
    zoneId_ = 0;
}

} // namespace req::zone
