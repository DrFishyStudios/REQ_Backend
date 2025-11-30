#include "../include/req/shared/Config.h"
#include "../include/req/shared/Logger.h"

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

    WorldRules cfg;
    
    // Top-level required and optional fields
    cfg.rulesetId = getRequired<std::string>(j, "ruleset_id", "WorldRules");
    cfg.displayName = getOrDefault<std::string>(j, "display_name", cfg.rulesetId);
    cfg.description = getOrDefault<std::string>(j, "description", "");

    // Parse XP rules
    if (j.contains("xp") && j["xp"].is_object()) {
        const auto& xpJson = j["xp"];
        cfg.xp.baseRate = getRequired<float>(xpJson, "base_rate", "WorldRules.xp");
        cfg.xp.groupBonusPerMember = getOrDefault<float>(xpJson, "group_bonus_per_member", 0.0f);
        cfg.xp.hotZoneMultiplierDefault = getOrDefault<float>(xpJson, "hot_zone_multiplier_default", 1.0f);
    } else {
        // If xp section is missing, log warning and use defaults
        logWarn("Config", "WorldRules missing 'xp' section, using defaults");
    }

    // Parse loot rules
    if (j.contains("loot") && j["loot"].is_object()) {
        const auto& lootJson = j["loot"];
        cfg.loot.dropRateMultiplier = getOrDefault<float>(lootJson, "drop_rate_multiplier", 1.0f);
        cfg.loot.coinRateMultiplier = getOrDefault<float>(lootJson, "coin_rate_multiplier", 1.0f);
        cfg.loot.rareDropMultiplier = getOrDefault<float>(lootJson, "rare_drop_multiplier", 1.0f);
    }

    // Parse death rules
    if (j.contains("death") && j["death"].is_object()) {
        const auto& deathJson = j["death"];
        cfg.death.xpLossMultiplier = getOrDefault<float>(deathJson, "xp_loss_multiplier", 1.0f);
        cfg.death.corpseRunEnabled = getOrDefault<bool>(deathJson, "corpse_run_enabled", true);
        // Also check for "corpse_runs" as alias (actual JSON uses this)
        if (deathJson.contains("corpse_runs") && !deathJson.contains("corpse_run_enabled")) {
            cfg.death.corpseRunEnabled = getOrDefault<bool>(deathJson, "corpse_runs", true);
        }
        cfg.death.corpseDecayMinutes = getOrDefault<int>(deathJson, "corpse_decay_minutes", 30);
    }

    // Parse UI helpers (check both "ui_helpers" and "qol" as actual JSON uses "qol")
    const json* uiJson = nullptr;
    if (j.contains("ui_helpers") && j["ui_helpers"].is_object()) {
        uiJson = &j["ui_helpers"];
    } else if (j.contains("qol") && j["qol"].is_object()) {
        uiJson = &j["qol"];
    }
    
    if (uiJson != nullptr) {
        // Check both naming conventions
        cfg.uiHelpers.conColorsEnabled = getOrDefault<bool>(*uiJson, "con_colors_enabled", true);
        if (uiJson->contains("con_outlines_enabled") && !uiJson->contains("con_colors_enabled")) {
            cfg.uiHelpers.conColorsEnabled = getOrDefault<bool>(*uiJson, "con_outlines_enabled", true);
        }
        
        cfg.uiHelpers.minimapEnabled = getOrDefault<bool>(*uiJson, "minimap_enabled", true);
        cfg.uiHelpers.questTrackerEnabled = getOrDefault<bool>(*uiJson, "quest_tracker_enabled", true);
        cfg.uiHelpers.corpseArrowEnabled = getOrDefault<bool>(*uiJson, "corpse_arrow_enabled", true);
    }

    // Parse hot zones array
    if (j.contains("hot_zones") && j["hot_zones"].is_array()) {
        for (const auto& hzJson : j["hot_zones"]) {
            WorldRules::HotZone hz;
            
            hz.zoneId = getRequired<std::uint32_t>(hzJson, "zone_id", "HotZone entry");
            hz.xpMultiplier = getRequired<float>(hzJson, "xp_multiplier", "HotZone entry");
            hz.lootMultiplier = getRequired<float>(hzJson, "loot_multiplier", "HotZone entry");
            
            // Handle start_date (null or string)
            if (hzJson.contains("start_date") && !hzJson["start_date"].is_null()) {
                hz.startDate = hzJson["start_date"].get<std::string>();
            } else {
                hz.startDate = "";
            }
            
            // Handle end_date (null or string)
            if (hzJson.contains("end_date") && !hzJson["end_date"].is_null()) {
                hz.endDate = hzJson["end_date"].get<std::string>();
            } else {
                hz.endDate = "";
            }
            
            cfg.hotZones.push_back(hz);
        }
    }

    logInfo("Config", std::string{"WorldRules loaded: rulesetId="} + cfg.rulesetId +
                       ", displayName=" + cfg.displayName +
                       ", hotZones=" + std::to_string(cfg.hotZones.size()));

    return cfg;
}

} // namespace req::shared
