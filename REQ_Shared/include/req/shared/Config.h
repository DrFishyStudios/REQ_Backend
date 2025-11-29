#pragma once

#include <string>
#include <vector>
#include "Types.h"

namespace req::shared {

struct LoginConfig {
    std::string address{ "0.0.0.0" };
    std::uint16_t port{ 7777 };
    std::string motd;
};

struct LoginWorldEntry {
    std::uint32_t worldId{ 0 };
    std::string worldName;
    std::string host;
    std::uint16_t port{ 0 };
    std::string rulesetId;
};

struct WorldListConfig {
    std::vector<LoginWorldEntry> worlds;
};

struct WorldZoneConfig {
    std::uint32_t zoneId{ 0 };
    std::string zoneName;
    std::string host;
    std::uint16_t port{ 0 };
    
    // Optional auto-launch fields
    std::string executablePath;
    std::vector<std::string> args;
};

struct WorldConfig {
    std::uint32_t worldId{ 0 };
    std::string worldName;
    std::string address{ "0.0.0.0" };
    std::uint16_t port{ 7778 };
    std::string rulesetId;
    bool autoLaunchZones{ false };
    std::vector<WorldZoneConfig> zones;
};

struct ZoneConfig {
    std::uint32_t zoneId{ 0 };
    std::string zoneName;
    
    // Safe spawn point
    float safeX{ 0.0f };
    float safeY{ 0.0f };
    float safeZ{ 0.0f };
    float safeYaw{ 0.0f };
    
    // Position auto-save interval (seconds)
    float autosaveIntervalSec{ 30.0f };
    
    // Interest management (snapshot filtering)
    bool broadcastFullState{ true };        // If true, send all players; if false, use interestRadius
    float interestRadius{ 2000.0f };        // Distance threshold for including players
    bool debugInterest{ false };            // Enable debug logging for interest filtering
};

LoginConfig loadLoginConfig(const std::string& path);
WorldListConfig loadWorldListConfig(const std::string& path);
WorldConfig loadWorldConfig(const std::string& path);
ZoneConfig loadZoneConfig(const std::string& path);

} // namespace req::shared
