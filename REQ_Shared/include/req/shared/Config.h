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

LoginConfig loadLoginConfig(const std::string& path);
WorldListConfig loadWorldListConfig(const std::string& path);
WorldConfig loadWorldConfig(const std::string& path);

} // namespace req::shared
