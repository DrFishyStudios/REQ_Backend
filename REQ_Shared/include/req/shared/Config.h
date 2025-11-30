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
    
    // Movement speed (units per second, server-authoritative)
    float moveSpeed{ 70.0f };               // Default: 70 uu/s (10x the old 7.0)
    
    // Position auto-save interval (seconds)
    float autosaveIntervalSec{ 30.0f };
    
    // Interest management (snapshot filtering)
    bool broadcastFullState{ true };        // If true, send all players; if false, use interestRadius
    float interestRadius{ 2000.0f };        // Distance threshold for including players
    bool debugInterest{ false };            // Enable debug logging for interest filtering
};

struct WorldRules {
    struct XpRules {
        float baseRate{1.0f};
        float groupBonusPerMember{0.0f};
        float hotZoneMultiplierDefault{1.0f};
    };

    struct LootRules {
        float dropRateMultiplier{1.0f};
        float coinRateMultiplier{1.0f};
        float rareDropMultiplier{1.0f};
    };

    struct DeathRules {
        float xpLossMultiplier{1.0f};
        bool corpseRunEnabled{true};
        int corpseDecayMinutes{30};
    };

    struct UiHelpers {
        bool conColorsEnabled{true};
        bool minimapEnabled{true};
        bool questTrackerEnabled{true};
        bool corpseArrowEnabled{true};
    };

    struct HotZone {
        std::uint32_t zoneId{0};
        float xpMultiplier{1.0f};
        float lootMultiplier{1.0f};
        // For now just store raw strings; parsing dates is out of scope for this task.
        std::string startDate; // may be empty if null
        std::string endDate;   // may be empty if null
    };

    std::string rulesetId;
    std::string displayName;
    std::string description;

    XpRules xp;
    LootRules loot;
    DeathRules death;
    UiHelpers uiHelpers;
    std::vector<HotZone> hotZones;
};

LoginConfig loadLoginConfig(const std::string& path);
WorldListConfig loadWorldListConfig(const std::string& path);
WorldConfig loadWorldConfig(const std::string& path);
ZoneConfig loadZoneConfig(const std::string& path);
WorldRules loadWorldRules(const std::string& path);

} // namespace req::shared
