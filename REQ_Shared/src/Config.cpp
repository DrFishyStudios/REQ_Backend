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

} // namespace req::shared
