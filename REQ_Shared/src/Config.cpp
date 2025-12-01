#include "../include/req/shared/Config.h"
#include "../include/req/shared/Logger.h"
#include "../include/req/shared/DataModels.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

// Include nlohmann/json from thirdparty folder
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

namespace req::shared {

namespace {
    // Helper: safely get a JSON value with type checking and default
    template<typename T>
    T getOrDefault(const json& j, const std::string& key, const T& defaultValue) {
        if (j.contains(key)) {
            try {
                return j[key].get<T>();
            } catch (const json::exception& e) {
                logWarn("Config", std::string{"Failed to parse key '"} + key + "': " + e.what() + " (using default)");
                return defaultValue;
            }
        }
        return defaultValue;
    }

    // Helper: safely get a required JSON value
    template<typename T>
    T getRequired(const json& j, const std::string& key, const std::string& configName) {
        if (!j.contains(key)) {
            std::string msg = std::string{"Missing required field '"} + key + "' in " + configName;
            logError("Config", msg);
            throw std::runtime_error(msg);
        }
        try {
            return j[key].get<T>();
        } catch (const json::exception& e) {
            std::string msg = std::string{"Failed to parse required field '"} + key + "' in " + configName + ": " + e.what();
            logError("Config", msg);
            throw std::runtime_error(msg);
        }
    }
}

LoginConfig loadLoginConfig(const std::string& path) {
    logInfo("Config", std::string{"Loading LoginConfig from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open LoginConfig file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse LoginConfig JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    LoginConfig cfg;
    cfg.address = getOrDefault<std::string>(j, "address", "0.0.0.0");
    cfg.port = getOrDefault<std::uint16_t>(j, "port", 7777);
    cfg.motd = getOrDefault<std::string>(j, "motd", "");

    // Validation
    if (cfg.port == 0 || cfg.port > 65535) {
        std::string msg = std::string{"Invalid port in LoginConfig: "} + std::to_string(cfg.port);
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    logInfo("Config", std::string{"LoginConfig loaded: address="} + cfg.address + 
            ", port=" + std::to_string(cfg.port));
    return cfg;
}

WorldListConfig loadWorldListConfig(const std::string& path) {
    logInfo("Config", std::string{"Loading WorldListConfig from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open WorldListConfig file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse WorldListConfig JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    WorldListConfig cfg;

    // Parse worlds array
    if (!j.contains("worlds") || !j["worlds"].is_array()) {
        std::string msg = "Missing or invalid 'worlds' array in WorldListConfig";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    const auto& worldsArray = j["worlds"];
    if (worldsArray.empty()) {
        std::string msg = "WorldListConfig must define at least one world";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    for (const auto& worldJson : worldsArray) {
        LoginWorldEntry world;
        world.worldId = getRequired<std::uint32_t>(worldJson, "world_id", "World entry");
        world.worldName = getRequired<std::string>(worldJson, "world_name", "World entry");
        world.host = getRequired<std::string>(worldJson, "host", "World entry");
        world.port = getRequired<std::uint16_t>(worldJson, "port", "World entry");
        world.rulesetId = getRequired<std::string>(worldJson, "ruleset_id", "World entry");

        // Validate port
        if (world.port == 0 || world.port > 65535) {
            std::string msg = std::string{"Invalid port for world '"} + world.worldName + "': " + std::to_string(world.port);
            logError("Config", msg);
            throw std::runtime_error(msg);
        }

        // Validate worldName not empty
        if (world.worldName.empty()) {
            std::string msg = "World entry must have non-empty world_name";
            logError("Config", msg);
            throw std::runtime_error(msg);
        }

        cfg.worlds.push_back(world);
    }

    logInfo("Config", std::string{"WorldListConfig loaded: "} + std::to_string(cfg.worlds.size()) + " world(s)");
    
    // Log each world for visibility
    for (const auto& world : cfg.worlds) {
        logInfo("Config", std::string{"  World: id="} + std::to_string(world.worldId) + 
                ", name=" + world.worldName + 
                ", endpoint=" + world.host + ":" + std::to_string(world.port) +
                ", ruleset=" + world.rulesetId);
    }

    return cfg;
}

WorldConfig loadWorldConfig(const std::string& path) {
    logInfo("Config", std::string{"Loading WorldConfig from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open WorldConfig file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse WorldConfig JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    WorldConfig cfg;
    cfg.worldId = getRequired<std::uint32_t>(j, "world_id", "WorldConfig");
    cfg.worldName = getRequired<std::string>(j, "world_name", "WorldConfig");
    cfg.address = getRequired<std::string>(j, "address", "WorldConfig");
    cfg.port = getRequired<std::uint16_t>(j, "port", "WorldConfig");
    cfg.rulesetId = getRequired<std::string>(j, "ruleset_id", "WorldConfig");
    cfg.autoLaunchZones = getOrDefault<bool>(j, "auto_launch_zones", false);

    // Parse zones array
    if (!j.contains("zones") || !j["zones"].is_array()) {
        std::string msg = "Missing or invalid 'zones' array in WorldConfig";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    const auto& zonesArray = j["zones"];
    if (zonesArray.empty()) {
        std::string msg = "WorldConfig must define at least one zone";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    for (const auto& zoneJson : zonesArray) {
        WorldZoneConfig zone;
        zone.zoneId = getRequired<std::uint32_t>(zoneJson, "zone_id", "Zone entry");
        zone.zoneName = getRequired<std::string>(zoneJson, "zone_name", "Zone entry");
        zone.host = getRequired<std::string>(zoneJson, "host", "Zone entry");
        zone.port = getRequired<std::uint16_t>(zoneJson, "port", "Zone entry");

        // Optional auto-launch fields
        zone.executablePath = getOrDefault<std::string>(zoneJson, "executable_path", "");
        if (zoneJson.contains("args") && zoneJson["args"].is_array()) {
            for (const auto& arg : zoneJson["args"]) {
                if (arg.is_string()) {
                    zone.args.push_back(arg.get<std::string>());
                }
            }
        }

        // Validate zone port
        if (zone.port == 0 || zone.port > 65535) {
            std::string msg = std::string{"Invalid port for zone '"} + zone.zoneName + "': " + std::to_string(zone.port);
            logError("Config", msg);
            throw std::runtime_error(msg);
        }

        cfg.zones.push_back(zone);
    }

    // Validation
    if (cfg.port == 0 || cfg.port > 65535) {
        std::string msg = std::string{"Invalid port in WorldConfig: "} + std::to_string(cfg.port);
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    if (cfg.worldName.empty()) {
        std::string msg = "WorldConfig worldName cannot be empty";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    logInfo("Config", std::string{"WorldConfig loaded: worldId="} + std::to_string(cfg.worldId) + 
            ", worldName=" + cfg.worldName + 
            ", address=" + cfg.address + 
            ", port=" + std::to_string(cfg.port) + 
            ", rulesetId=" + cfg.rulesetId +
            ", zones=" + std::to_string(cfg.zones.size()) + 
            ", autoLaunchZones=" + (cfg.autoLaunchZones ? "true" : "false"));
    
    // Log each zone for auto-launch visibility
    for (const auto& zone : cfg.zones) {
        logInfo("Config", std::string{"  Zone: id="} + std::to_string(zone.zoneId) + 
                ", name=" + zone.zoneName + 
                ", endpoint=" + zone.host + ":" + std::to_string(zone.port) +
                ", executable=" + (zone.executablePath.empty() ? "<none>" : zone.executablePath));
    }

    return cfg;
}

ZoneConfig loadZoneConfig(const std::string& path) {
    logInfo("Config", std::string{"Loading ZoneConfig from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open ZoneConfig file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse ZoneConfig JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    ZoneConfig cfg;
    cfg.zoneId = getRequired<std::uint32_t>(j, "zone_id", "ZoneConfig");
    cfg.zoneName = getRequired<std::string>(j, "zone_name", "ZoneConfig");
    
    // Safe spawn point (optional, defaults to 0,0,0)
    if (j.contains("safe_spawn") && j["safe_spawn"].is_object()) {
        const auto& spawn = j["safe_spawn"];
        cfg.safeX = getOrDefault<float>(spawn, "x", 0.0f);
        cfg.safeY = getOrDefault<float>(spawn, "y", 0.0f);
        cfg.safeZ = getOrDefault<float>(spawn, "z", 0.0f);
        cfg.safeYaw = getOrDefault<float>(spawn, "yaw", 0.0f);
    }
    
    // Movement speed (optional, default 70.0 uu/s)
    cfg.moveSpeed = getOrDefault<float>(j, "move_speed", 70.0f);
    
    // Auto-save interval (optional, default 30s)
    cfg.autosaveIntervalSec = getOrDefault<float>(j, "autosave_interval_sec", 30.0f);
    
    // Interest management (optional, defaults: broadcast_full_state=true, interest_radius=2000.0)
    cfg.broadcastFullState = getOrDefault<bool>(j, "broadcast_full_state", true);
    cfg.interestRadius = getOrDefault<float>(j, "interest_radius", 2000.0f);
    cfg.debugInterest = getOrDefault<bool>(j, "debug_interest", false);
    
    // Validation
    if (cfg.moveSpeed <= 0.0f) {
        std::string msg = std::string{"Invalid move_speed in ZoneConfig: "} + std::to_string(cfg.moveSpeed);
        logError("Config", msg);
        throw std::runtime_error(msg);
    }
    
    if (cfg.autosaveIntervalSec <= 0.0f) {
        std::string msg = std::string{"Invalid autosave_interval_sec in ZoneConfig: "} + std::to_string(cfg.autosaveIntervalSec);
        logError("Config", msg);
        throw std::runtime_error(msg);
    }
    
    if (cfg.interestRadius < 0.0f) {
        std::string msg = std::string{"Invalid interest_radius in ZoneConfig: "} + std::to_string(cfg.interestRadius);
        logError("Config", msg);
        throw std::runtime_error(msg);
    }
    
    if (cfg.zoneName.empty()) {
        std::string msg = "ZoneConfig zoneName cannot be empty";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    logInfo("Config", std::string{"ZoneConfig loaded: zoneId="} + std::to_string(cfg.zoneId) + 
            ", zoneName=" + cfg.zoneName + 
            ", safeSpawn=(" + std::to_string(cfg.safeX) + "," + std::to_string(cfg.safeY) + "," + std::to_string(cfg.safeZ) + ")" +
            ", moveSpeed=" + std::to_string(cfg.moveSpeed) +
            ", autosaveIntervalSec=" + std::to_string(cfg.autosaveIntervalSec) +
            ", broadcastFullState=" + (cfg.broadcastFullState ? "true" : "false") +
            ", interestRadius=" + std::to_string(cfg.interestRadius) +
            ", debugInterest=" + (cfg.debugInterest ? "true" : "false"));
    
    return cfg;
}

WorldRules loadWorldRules(const std::string& path) {
    logInfo("Config", std::string{"Loading WorldRules from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open WorldRules file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse WorldRules JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    WorldRules rules;
    
    // Top-level required fields
    rules.rulesetId = getRequired<std::string>(j, "ruleset_id", "WorldRules");
    rules.displayName = getOrDefault<std::string>(j, "display_name", rules.rulesetId);
    rules.description = getOrDefault<std::string>(j, "description", "");
    
    // XP rules
    if (j.contains("xp") && j["xp"].is_object()) {
        const auto& xp = j["xp"];
        rules.xp.baseRate = getOrDefault<float>(xp, "base_rate", 1.0f);
        rules.xp.groupBonusPerMember = getOrDefault<float>(xp, "group_bonus_per_member", 0.0f);
        rules.xp.hotZoneMultiplierDefault = getOrDefault<float>(xp, "hot_zone_multiplier_default", 1.0f);
    }
    
    // Loot rules
    if (j.contains("loot") && j["loot"].is_object()) {
        const auto& loot = j["loot"];
        rules.loot.dropRateMultiplier = getOrDefault<float>(loot, "drop_rate_multiplier", 1.0f);
        rules.loot.coinRateMultiplier = getOrDefault<float>(loot, "coin_rate_multiplier", 1.0f);
        rules.loot.rareDropMultiplier = getOrDefault<float>(loot, "rare_drop_multiplier", 1.0f);
    }
    
    // Death rules
    if (j.contains("death") && j["death"].is_object()) {
        const auto& death = j["death"];
        rules.death.xpLossMultiplier = getOrDefault<float>(death, "xp_loss_multiplier", 1.0f);
        
        // Handle both "corpse_runs" and "corpse_run_enabled"
        if (death.contains("corpse_runs")) {
            rules.death.corpseRunEnabled = getOrDefault<bool>(death, "corpse_runs", true);
        } else {
            rules.death.corpseRunEnabled = getOrDefault<bool>(death, "corpse_run_enabled", true);
        }
        
        rules.death.corpseDecayMinutes = getOrDefault<int>(death, "corpse_decay_minutes", 30);
    }
    
    // UI helpers (can be named "qol" or "ui_helpers")
    const json* uiSection = nullptr;
    if (j.contains("qol") && j["qol"].is_object()) {
        uiSection = &j["qol"];
    } else if (j.contains("ui_helpers") && j["ui_helpers"].is_object()) {
        uiSection = &j["ui_helpers"];
    }
    
    if (uiSection) {
        // Handle both "con_outlines_enabled" and "con_colors_enabled"
        if (uiSection->contains("con_outlines_enabled")) {
            rules.uiHelpers.conColorsEnabled = getOrDefault<bool>(*uiSection, "con_outlines_enabled", true);
        } else {
            rules.uiHelpers.conColorsEnabled = getOrDefault<bool>(*uiSection, "con_colors_enabled", true);
        }
        
        rules.uiHelpers.minimapEnabled = getOrDefault<bool>(*uiSection, "minimap_enabled", true);
        rules.uiHelpers.questTrackerEnabled = getOrDefault<bool>(*uiSection, "quest_tracker_enabled", true);
        rules.uiHelpers.corpseArrowEnabled = getOrDefault<bool>(*uiSection, "corpse_arrow_enabled", true);
        rules.uiHelpers.factionColorPulsesEnabled = getOrDefault<bool>(*uiSection, "faction_color_pulses_enabled", true);
    }
    
    // Hot zones
    if (j.contains("hot_zones") && j["hot_zones"].is_array()) {
        for (const auto& hzJson : j["hot_zones"]) {
            WorldRules::HotZone hz;
            hz.zoneId = getRequired<std::uint32_t>(hzJson, "zone_id", "Hot zone entry");
            hz.xpMultiplier = getOrDefault<float>(hzJson, "xp_multiplier", 1.0f);
            hz.lootMultiplier = getOrDefault<float>(hzJson, "loot_multiplier", 1.0f);
            
            // Handle null dates as empty strings
            if (hzJson.contains("start_date") && !hzJson["start_date"].is_null()) {
                hz.startDate = hzJson["start_date"].get<std::string>();
            }
            if (hzJson.contains("end_date") && !hzJson["end_date"].is_null()) {
                hz.endDate = hzJson["end_date"].get<std::string>();
            }
            
            rules.hotZones.push_back(hz);
        }
    }
    
    logInfo("Config", std::string{"WorldRules loaded: rulesetId="} + rules.rulesetId + 
            ", displayName=" + rules.displayName + 
            ", hotZones=" + std::to_string(rules.hotZones.size()));
    
    return rules;
}

XpTable loadDefaultXpTable(const std::string& path) {
    logInfo("Config", std::string{"Loading XpTable from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open XpTable file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse XpTable JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    // Expect a "tables" array with at least one table
    if (!j.contains("tables") || !j["tables"].is_array() || j["tables"].empty()) {
        std::string msg = "XpTable file must contain non-empty 'tables' array";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    // Load first table as default
    const auto& tableJson = j["tables"][0];
    
    XpTable table;
    table.id = getRequired<std::string>(tableJson, "id", "XpTable");
    table.displayName = getOrDefault<std::string>(tableJson, "display_name", table.id);
    
    // Parse entries
    if (!tableJson.contains("entries") || !tableJson["entries"].is_array()) {
        std::string msg = "XpTable must contain 'entries' array";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }
    
    const auto& entriesArray = tableJson["entries"];
    if (entriesArray.empty()) {
        std::string msg = "XpTable entries array cannot be empty";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }
    
    for (const auto& entryJson : entriesArray) {
        XpTableEntry entry;
        entry.level = getRequired<int>(entryJson, "level", "XpTableEntry");
        entry.totalXp = getRequired<std::int64_t>(entryJson, "total_xp", "XpTableEntry");
        
        // Validation
        if (entry.level <= 0) {
            std::string msg = std::string{"XpTable entry has invalid level: "} + std::to_string(entry.level);
            logError("Config", msg);
            throw std::runtime_error(msg);
        }
        
        if (entry.totalXp < 0) {
            std::string msg = std::string{"XpTable entry has negative totalXp: "} + std::to_string(entry.totalXp);
            logError("Config", msg);
            throw std::runtime_error(msg);
        }
        
        table.entries.push_back(entry);
    }
    
    // Validate entries are sorted by level
    for (std::size_t i = 1; i < table.entries.size(); ++i) {
        if (table.entries[i].level <= table.entries[i - 1].level) {
            std::string msg = "XpTable entries must be sorted by ascending level";
            logError("Config", msg);
            throw std::runtime_error(msg);
        }
        
        if (table.entries[i].totalXp < table.entries[i - 1].totalXp) {
            std::string msg = "XpTable totalXp must be non-decreasing";
            logError("Config", msg);
            throw std::runtime_error(msg);
        }
    }
    
    // Validate levels are contiguous starting from 1
    if (table.entries[0].level != 1) {
        std::string msg = "XpTable must start at level 1";
        logError("Config", msg);
        throw std::runtime_error(msg);
    }
    
    for (std::size_t i = 1; i < table.entries.size(); ++i) {
        if (table.entries[i].level != table.entries[i - 1].level + 1) {
            std::string msg = std::string{"XpTable has non-contiguous levels at index "} + std::to_string(i);
            logError("Config", msg);
            throw std::runtime_error(msg);
        }
    }
    
    logInfo("Config", std::string{"XpTable loaded: id="} + table.id + 
            ", displayName=" + table.displayName + 
            ", levels=1-" + std::to_string(table.entries.back().level));
    
    return table;
}

std::int64_t GetTotalXpForLevel(const XpTable& table, int level) {
    if (level <= 0) {
        logWarn("Config", std::string{"GetTotalXpForLevel: Invalid level "} + std::to_string(level) + ", returning 0");
        return 0;
    }
    
    if (table.entries.empty()) {
        logWarn("Config", "GetTotalXpForLevel: Empty XP table, returning 0");
        return 0;
    }
    
    // Clamp to table range
    if (level < table.entries.front().level) {
        return table.entries.front().totalXp;
    }
    
    if (level > table.entries.back().level) {
        logWarn("Config", std::string{"GetTotalXpForLevel: Level "} + std::to_string(level) + 
                " exceeds max level " + std::to_string(table.entries.back().level) + ", clamping");
        return table.entries.back().totalXp;
    }
    
    // Find entry for level (entries are sorted and contiguous)
    int index = level - table.entries.front().level;
    if (index >= 0 && index < static_cast<int>(table.entries.size())) {
        return table.entries[index].totalXp;
    }
    
    // Fallback: binary search
    for (const auto& entry : table.entries) {
        if (entry.level == level) {
            return entry.totalXp;
        }
    }
    
    logWarn("Config", std::string{"GetTotalXpForLevel: Could not find level "} + std::to_string(level) + ", returning 0");
    return 0;
}

void AddXp(req::shared::data::Character& character,
           std::int64_t amount,
           const XpTable& xpTable,
           const WorldRules& rules) {
    if (amount <= 0) {
        return; // No XP to add
    }
    
    if (xpTable.entries.empty()) {
        logWarn("XP", "AddXp: XP table is empty, cannot add XP");
        return;
    }
    
    const int maxLevel = xpTable.entries.back().level;
    
    // Check if already at max level
    if (character.level >= maxLevel) {
        logInfo("XP", std::string{"Character "} + std::to_string(character.characterId) + 
                " is already at max level " + std::to_string(maxLevel));
        return;
    }
    
    // Apply base rate multiplier
    std::int64_t adjustedAmount = static_cast<std::int64_t>(amount * rules.xp.baseRate);
    
    std::uint32_t oldLevel = character.level;
    std::uint64_t oldXp = character.xp;
    
    // Add XP
    character.xp += adjustedAmount;
    
    // Check for level-ups (loop to handle multiple levels)
    bool leveledUp = false;
    while (character.level < maxLevel) {
        std::int64_t xpForNextLevel = GetTotalXpForLevel(xpTable, character.level + 1);
        
        if (character.xp >= static_cast<std::uint64_t>(xpForNextLevel)) {
            ++character.level;
            leveledUp = true;
            
            logInfo("XP", std::string{"[LEVELUP] Character "} + std::to_string(character.characterId) + 
                    " (" + character.name + ") reached level " + std::to_string(character.level) + 
                    " (XP: " + std::to_string(character.xp) + ")");
        } else {
            break; // No more level-ups
        }
    }
    
    // Log XP gain
    if (leveledUp) {
        logInfo("XP", std::string{"[XP] Character "} + std::to_string(character.characterId) + 
                " leveled up: " + std::to_string(oldLevel) + " -> " + std::to_string(character.level) + 
                ", XP: " + std::to_string(oldXp) + " -> " + std::to_string(character.xp));
    } else {
        logInfo("XP", std::string{"[XP] Character "} + std::to_string(character.characterId) + 
                " gained " + std::to_string(adjustedAmount) + " XP (now " + std::to_string(character.xp) + 
                " / " + std::to_string(GetTotalXpForLevel(xpTable, character.level + 1)) + 
                " for level " + std::to_string(character.level + 1) + ")");
    }
}

// ============================================================================
// NPC Template Loader (Phase 2)
// ============================================================================

data::NpcTemplateStore loadNpcTemplates(const std::string& path) {
    logInfo("Config", std::string{"Loading NPC templates from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open NPC templates file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse NPC templates JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    data::NpcTemplateStore store;

    // Parse npcs array
    if (!j.contains("npcs") || !j["npcs"].is_array()) {
        logWarn("Config", "NPC templates file does not contain 'npcs' array, returning empty store");
        return store;
    }

    const auto& npcsArray = j["npcs"];
    int loadedCount = 0;

    for (const auto& npcJson : npcsArray) {
        data::NpcTemplate npcTemplate;

        // Required fields
        npcTemplate.id = getRequired<std::int32_t>(npcJson, "id", "NPC template");
        npcTemplate.name = getRequired<std::string>(npcJson, "name", "NPC template");

        // Optional fields with defaults
        npcTemplate.archetype = getOrDefault<std::string>(npcJson, "archetype", "");
        npcTemplate.factionId = getOrDefault<std::int32_t>(npcJson, "faction_id", 0);
        npcTemplate.lootTableId = getOrDefault<std::int32_t>(npcJson, "loot_table_id", 0);

        // Parse stat_block
        if (npcJson.contains("stat_block") && npcJson["stat_block"].is_object()) {
            const auto& stats = npcJson["stat_block"];
            npcTemplate.stats.levelMin = getOrDefault<int>(stats, "level_min", 1);
            npcTemplate.stats.levelMax = getOrDefault<int>(stats, "level_max", npcTemplate.stats.levelMin);
            npcTemplate.stats.hp = getOrDefault<int>(stats, "hp", 100);
            npcTemplate.stats.mana = getOrDefault<int>(stats, "mana", 0);
            npcTemplate.stats.ac = getOrDefault<int>(stats, "ac", 10);
            npcTemplate.stats.atk = getOrDefault<int>(stats, "atk", 10);
            npcTemplate.stats.str = getOrDefault<int>(stats, "str", 10);
            npcTemplate.stats.sta = getOrDefault<int>(stats, "sta", 10);
            npcTemplate.stats.dex = getOrDefault<int>(stats, "dex", 10);
            npcTemplate.stats.agi = getOrDefault<int>(stats, "agi", 10);
            npcTemplate.stats.intl = getOrDefault<int>(stats, "int", 10);
            npcTemplate.stats.wis = getOrDefault<int>(stats, "wis", 10);
            npcTemplate.stats.cha = getOrDefault<int>(stats, "cha", 10);
        }

        // Parse behavior_flags
        if (npcJson.contains("behavior_flags") && npcJson["behavior_flags"].is_object()) {
            const auto& flags = npcJson["behavior_flags"];
            npcTemplate.behaviorFlags.isRoamer = getOrDefault<bool>(flags, "is_roamer", false);
            npcTemplate.behaviorFlags.isStatic = getOrDefault<bool>(flags, "is_static", true);
            npcTemplate.behaviorFlags.isSocial = getOrDefault<bool>(flags, "is_social", false);
            npcTemplate.behaviorFlags.usesRanged = getOrDefault<bool>(flags, "uses_ranged", false);
            npcTemplate.behaviorFlags.callsForHelp = getOrDefault<bool>(flags, "calls_for_help", false);
            npcTemplate.behaviorFlags.canFlee = getOrDefault<bool>(flags, "can_flee", false);
            npcTemplate.behaviorFlags.immuneMez = getOrDefault<bool>(flags, "immune_mez", false);
            npcTemplate.behaviorFlags.immuneCharm = getOrDefault<bool>(flags, "immune_charm", false);
            npcTemplate.behaviorFlags.immuneFear = getOrDefault<bool>(flags, "immune_fear", false);
            npcTemplate.behaviorFlags.leashToSpawn = getOrDefault<bool>(flags, "leash_to_spawn", true);
        }

        // Parse behavior_params
        if (npcJson.contains("behavior_params") && npcJson["behavior_params"].is_object()) {
            const auto& params = npcJson["behavior_params"];
            npcTemplate.behaviorParams.aggroRadius = getOrDefault<float>(params, "aggro_radius", 800.0f);
            npcTemplate.behaviorParams.socialRadius = getOrDefault<float>(params, "social_radius", 600.0f);
            npcTemplate.behaviorParams.fleeHealthPercent = getOrDefault<float>(params, "flee_health_percent", 0.0f);
            npcTemplate.behaviorParams.leashRadius = getOrDefault<float>(params, "leash_radius", 2000.0f);
            npcTemplate.behaviorParams.leashTimeoutSec = getOrDefault<float>(params, "leash_timeout_sec", 10.0f);
            npcTemplate.behaviorParams.maxChaseDistance = getOrDefault<float>(params, "max_chase_distance", 2500.0f);
            npcTemplate.behaviorParams.preferredRange = getOrDefault<float>(params, "preferred_range", 200.0f);
            npcTemplate.behaviorParams.assistDelaySec = getOrDefault<float>(params, "assist_delay_sec", 0.5f);
        }

        // Package IDs
        npcTemplate.visualId = getOrDefault<std::string>(npcJson, "visual_id", "");
        npcTemplate.abilityPackageId = getOrDefault<std::string>(npcJson, "ability_package_id", "");
        npcTemplate.navigationPackageId = getOrDefault<std::string>(npcJson, "navigation_package_id", "");
        npcTemplate.behaviorPackageId = getOrDefault<std::string>(npcJson, "behavior_package_id", "");

        // Validate ID
        if (npcTemplate.id == 0) {
            logWarn("Config", "Skipping NPC template with id=0 (invalid)");
            continue;
        }

        // Check for duplicate IDs
        if (store.templates.find(npcTemplate.id) != store.templates.end()) {
            logWarn("Config", std::string{"Duplicate NPC template ID: "} + std::to_string(npcTemplate.id) + ", skipping");
            continue;
        }

        // Add to store
        store.templates[npcTemplate.id] = npcTemplate;
        loadedCount++;

        logInfo("Config", std::string{"  Loaded NPC template: id="} + std::to_string(npcTemplate.id) +
                ", name=\"" + npcTemplate.name + "\"" +
                ", archetype=" + npcTemplate.archetype +
                ", level=" + std::to_string(npcTemplate.stats.levelMin) + "-" + std::to_string(npcTemplate.stats.levelMax));
    }

    logInfo("Config", std::string{"NPC templates loaded: "} + std::to_string(loadedCount) + " template(s)");
    return store;
}

// ============================================================================
// Spawn Table Loader (Phase 2)
// ============================================================================

data::SpawnTable loadSpawnTable(const std::string& path) {
    logInfo("Config", std::string{"Loading spawn table from: "} + path);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::string msg = std::string{"Failed to open spawn table file: "} + path;
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::string msg = std::string{"Failed to parse spawn table JSON from "} + path + ": " + e.what();
        logError("Config", msg);
        throw std::runtime_error(msg);
    }

    data::SpawnTable table;

    // Parse zone_id
    table.zoneId = getRequired<std::int32_t>(j, "zone_id", "Spawn table");

    // Parse spawn_groups (optional)
    if (j.contains("spawn_groups") && j["spawn_groups"].is_array()) {
        for (const auto& groupJson : j["spawn_groups"]) {
            data::SpawnGroup group;
            group.spawnGroupId = getRequired<std::int32_t>(groupJson, "spawn_group_id", "Spawn group");

            // Parse entries
            if (groupJson.contains("entries") && groupJson["entries"].is_array()) {
                for (const auto& entryJson : groupJson["entries"]) {
                    data::SpawnGroupEntry entry;
                    entry.npcId = getRequired<std::int32_t>(entryJson, "npc_id", "Spawn group entry");
                    entry.weight = getOrDefault<int>(entryJson, "weight", 1);

                    if (entry.weight <= 0) {
                        logWarn("Config", std::string{"Spawn group entry has invalid weight: "} + 
                                std::to_string(entry.weight) + ", using 1");
                        entry.weight = 1;
                    }

                    group.entries.push_back(entry);
                }
            }

            // Validate group has entries
            if (group.entries.empty()) {
                logWarn("Config", std::string{"Spawn group "} + std::to_string(group.spawnGroupId) + 
                        " has no entries, skipping");
                continue;
            }

            // Check for duplicate group IDs
            if (table.spawnGroups.find(group.spawnGroupId) != table.spawnGroups.end()) {
                logWarn("Config", std::string{"Duplicate spawn_group_id: "} + 
                        std::to_string(group.spawnGroupId) + ", skipping");
                continue;
            }

            table.spawnGroups[group.spawnGroupId] = group;
        }
    }

    // Parse spawns array
    if (!j.contains("spawns") || !j["spawns"].is_array()) {
        logWarn("Config", "Spawn table does not contain 'spawns' array, returning empty table");
        return table;
    }

    const auto& spawnsArray = j["spawns"];
    int loadedCount = 0;

    for (const auto& spawnJson : spawnsArray) {
        data::SpawnPoint spawn;

        // Required fields
        spawn.spawnId = getRequired<std::int32_t>(spawnJson, "spawn_id", "Spawn point");

        // Position
        if (spawnJson.contains("position") && spawnJson["position"].is_object()) {
            const auto& pos = spawnJson["position"];
            spawn.x = getOrDefault<float>(pos, "x", 0.0f);
            spawn.y = getOrDefault<float>(pos, "y", 0.0f);
            spawn.z = getOrDefault<float>(pos, "z", 0.0f);
            spawn.heading = getOrDefault<float>(pos, "heading", 0.0f);
        }

        // Spawn selection (group vs direct)
        spawn.spawnGroupId = getOrDefault<std::int32_t>(spawnJson, "spawn_group_id", 0);
        spawn.directNpcId = getOrDefault<std::int32_t>(spawnJson, "direct_npc_id", 0);

        // Validate: must have either spawn_group_id or direct_npc_id
        if (spawn.spawnGroupId == 0 && spawn.directNpcId == 0) {
            logWarn("Config", std::string{"Spawn point "} + std::to_string(spawn.spawnId) + 
                    " has neither spawn_group_id nor direct_npc_id, skipping");
            continue;
        }

        // Validate: if using spawn_group_id, ensure group exists
        if (spawn.spawnGroupId != 0) {
            if (table.spawnGroups.find(spawn.spawnGroupId) == table.spawnGroups.end()) {
                logWarn("Config", std::string{"Spawn point "} + std::to_string(spawn.spawnId) + 
                        " references non-existent spawn_group_id: " + std::to_string(spawn.spawnGroupId) + 
                        ", skipping");
                continue;
            }

            // If both are set, log warning and prefer group
            if (spawn.directNpcId != 0) {
                logWarn("Config", std::string{"Spawn point "} + std::to_string(spawn.spawnId) + 
                        " has both spawn_group_id and direct_npc_id, using spawn_group_id");
                spawn.directNpcId = 0;
            }
        }

        // Respawn parameters
        spawn.respawnTimeSec = getOrDefault<float>(spawnJson, "respawn_time_sec", 120.0f);
        spawn.respawnVarianceSec = getOrDefault<float>(spawnJson, "respawn_variance_sec", 0.0f);

        // Optional behaviors
        spawn.roamRadius = getOrDefault<float>(spawnJson, "roam_radius", 0.0f);
        spawn.namedChance = getOrDefault<float>(spawnJson, "named_chance", 0.0f);
        spawn.dayOnly = getOrDefault<bool>(spawnJson, "day_only", false);
        spawn.nightOnly = getOrDefault<bool>(spawnJson, "night_only", false);

        // Validate day/night exclusivity
        if (spawn.dayOnly && spawn.nightOnly) {
            logWarn("Config", std::string{"Spawn point "} + std::to_string(spawn.spawnId) + 
                    " has both day_only and night_only set, will never spawn!");
        }

        table.spawnPoints.push_back(spawn);
        loadedCount++;
    }

    logInfo("Config", std::string{"Spawn table loaded: zone="} + std::to_string(table.zoneId) +
            ", spawns=" + std::to_string(loadedCount) +
            ", spawn_groups=" + std::to_string(table.spawnGroups.size()));

    return table;
}

} // namespace req::shared
