# WorldRules Configuration - Implementation Summary

## Overview
Added `WorldRules` struct and loader to REQ_Shared config system for loading ruleset definitions from JSON files.

---

## Files Modified

### 1. REQ_Shared/include/req/shared/Config.h

**Added:**
- `struct WorldRules` with nested structs:
  - `XpRules` - XP rate configuration
  - `LootRules` - Loot drop rate configuration  
  - `DeathRules` - Death penalty configuration
  - `UiHelpers` - UI/QoL feature toggles
  - `HotZone` - Hot zone multiplier definitions
- `WorldRules loadWorldRules(const std::string& path);` declaration

**No changes to:**
- `LoginConfig`
- `WorldConfig`
- `ZoneConfig`
- Any network protocol types

---

### 2. REQ_Shared/src/Config.cpp

**Added:**
- `WorldRules loadWorldRules(const std::string& path)` implementation

**Uses existing helper functions:**
- `getRequired<T>()` - for required fields
- `getOrDefault<T>()` - for optional fields with defaults

**No changes to:**
- `loadLoginConfig()`
- `loadWorldConfig()`
- `loadZoneConfig()`

---

## WorldRules Structure

```cpp
struct WorldRules {
    // Nested configurations
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
        std::string startDate; // empty if null
        std::string endDate;   // empty if null
    };

    // Top-level fields
    std::string rulesetId;
    std::string displayName;
    std::string description;

    XpRules xp;
    LootRules loot;
    DeathRules death;
    UiHelpers uiHelpers;
    std::vector<HotZone> hotZones;
};
```

---

## JSON Format

**File:** `config/world_rules_classic_plus_qol.json`

```json
{
  "ruleset_id": "classic_plus_qol",
  "display_name": "Classic Norrath + QoL",
  "description": "Classic EverQuest-inspired pacing...",
  
  "xp": {
    "base_rate": 1.0,
    "group_bonus_per_member": 0.1,
    "hot_zone_multiplier_default": 1.0
  },
  
  "loot": {
    "drop_rate_multiplier": 1.0,
    "coin_rate_multiplier": 1.0,
    "rare_drop_multiplier": 1.0
  },
  
  "death": {
    "xp_loss_multiplier": 1.0,
    "corpse_runs": true,
    "corpse_decay_minutes": 1440
  },
  
  "qol": {
    "con_outlines_enabled": true,
    "minimap_enabled": true,
    "quest_tracker_enabled": true,
    "corpse_arrow_enabled": true
  },
  
  "hot_zones": [
    {
      "zone_id": 20,
      "xp_multiplier": 1.5,
      "loot_multiplier": 1.2,
      "start_date": null,
      "end_date": null
    }
  ]
}
```

---

## Field Mapping

### Required Fields
- `ruleset_id` ? `rulesetId` (string)
- `xp.base_rate` ? `xp.baseRate` (float)
- Hot zone entries:
  - `zone_id` ? `zoneId` (uint32)
  - `xp_multiplier` ? `xpMultiplier` (float)
  - `loot_multiplier` ? `lootMultiplier` (float)

### Optional Fields (with defaults)
- `display_name` ? `displayName` (defaults to `rulesetId`)
- `description` ? `description` (defaults to `""`)
- `xp.group_bonus_per_member` ? `xp.groupBonusPerMember` (default `0.0f`)
- `xp.hot_zone_multiplier_default` ? `xp.hotZoneMultiplierDefault` (default `1.0f`)
- All `loot.*` fields (default `1.0f`)
- All `death.*` fields (defaults: `1.0f`, `true`, `30`)
- All UI helper fields (default `true`)
- Hot zone dates (default `""` for null)

### Aliasing Support
The loader handles different naming conventions:
- `corpse_runs` OR `corpse_run_enabled` ? `corpseRunEnabled`
- `con_outlines_enabled` OR `con_colors_enabled` ? `conColorsEnabled`
- `qol` OR `ui_helpers` section ? `uiHelpers`

---

## Loader Behavior

### Success Path
1. Log: `Loading WorldRules from: <path>`
2. Open JSON file
3. Parse JSON
4. Extract required fields (throw if missing)
5. Extract optional fields (use defaults if missing)
6. Parse nested objects (`xp`, `loot`, `death`, `ui_helpers`/`qol`)
7. Parse `hot_zones` array (if present)
8. Log: `WorldRules loaded: rulesetId=<id>, displayName=<name>, hotZones=<count>`
9. Return `WorldRules` struct

### Error Handling
- **File not found:** Log error, throw `std::runtime_error`
- **JSON parse error:** Log error with exception details, throw `std::runtime_error`
- **Missing required field:** Log error, throw `std::runtime_error`
- **Invalid field type:** Log warning, use default value (for optional fields)

### Logging Examples
```
[INFO] [Config] Loading WorldRules from: config/world_rules_classic_plus_qol.json
[INFO] [Config] WorldRules loaded: rulesetId=classic_plus_qol, displayName=Classic Norrath + QoL, hotZones=1
```

---

## Usage Example

```cpp
#include "req/shared/Config.h"

// Load rules
auto rules = req::shared::loadWorldRules("config/world_rules_classic_plus_qol.json");

// Access fields
float xpRate = rules.xp.baseRate;
bool hasCorpseRuns = rules.death.corpseRunEnabled;
bool minimapOn = rules.uiHelpers.minimapEnabled;

// Check hot zones
for (const auto& hz : rules.hotZones) {
    if (hz.zoneId == currentZoneId) {
        xpMultiplier = hz.xpMultiplier;
        break;
    }
}
```

---

## Integration Points (Future)

### WorldServer
- Load `WorldRules` based on `WorldConfig::rulesetId`
- Pass rules to ZoneServers on launch
- Use for XP/loot calculations

### ZoneServer
- Store `WorldRules` instance
- Apply XP multipliers on kills
- Check hot zone bonuses
- Enforce death penalties
- Enable/disable UI features per client

### Client (Unreal)
- Receive ruleset info in zone welcome message
- Enable/disable UI features based on flags
- Display rule descriptions in server browser

---

## Testing

### Manual Test
```bash
# Compile test program
g++ -std=c++20 config/test_world_rules_loader.cpp -o test_world_rules

# Run test
./test_world_rules

# Expected output:
=== WorldRules Loaded Successfully ===
Ruleset ID: classic_plus_qol
Display Name: Classic Norrath + QoL
Description: Classic EverQuest-inspired pacing...
...
? All fields loaded successfully!
```

### Validation Checklist
- ? Compiles without errors
- ? Loads valid JSON successfully
- ? Handles missing optional fields with defaults
- ? Throws on missing required fields
- ? Throws on invalid JSON syntax
- ? Parses nested objects correctly
- ? Handles hot_zones array
- ? Handles null dates (empty strings)
- ? Logs appropriately

---

## No Changes Made To

- ? Network protocol (ProtocolSchemas.h/cpp)
- ? Zone movement code (ZoneServer movement logic)
- ? LoginConfig / WorldConfig / ZoneConfig structs
- ? Existing config loaders
- ? Message types or serialization
- ? Client/server handshake flow

---

## Build Status

**? Build Successful** - No compilation errors or warnings.

---

## Next Steps (Future Work)

1. **WorldServer Integration**
   - Load rules on startup based on `WorldConfig::rulesetId`
   - Store in WorldServer instance
   - Pass to ZoneServers

2. **ZoneServer Integration**
   - Accept WorldRules in constructor/config
   - Apply XP multipliers on NPC kills
   - Check hot zone bonuses
   - Use death rules for XP loss

3. **Combat Integration**
   - XP rewards using `xp.baseRate`
   - Group XP bonus using `xp.groupBonusPerMember`
   - Hot zone checks for bonus XP

4. **Loot Integration**
   - Drop rate adjustments
   - Coin drop adjustments
   - Rare drop rate tweaks

5. **Death Integration**
   - XP loss calculation
   - Corpse run mechanics
   - Corpse decay timers

6. **Client Integration**
   - UI feature toggles
   - Con color display
   - Minimap/quest tracker
   - Corpse locator arrow

---

## Summary

**What was added:**
- `WorldRules` struct with 5 nested sub-structs
- `loadWorldRules()` function with full JSON parsing
- Alias support for different naming conventions
- Comprehensive error handling and logging

**What was NOT changed:**
- Any existing config types
- Any network protocol code
- Any zone movement code
- Any server logic

**Status:** ? Complete and tested
**Build:** ? Successful
**Ready for:** Integration into WorldServer/ZoneServer

---

## Files Summary

| File | Status | Changes |
|------|--------|---------|
| `REQ_Shared/include/req/shared/Config.h` | Modified | Added `WorldRules` struct + declaration |
| `REQ_Shared/src/Config.cpp` | Modified | Added `loadWorldRules()` implementation |
| `config/world_rules_classic_plus_qol.json` | Existing | Reference JSON file (no changes) |
| `config/test_world_rules_loader.cpp` | New | Test program for validation |

---

## Complete! ??

The WorldRules configuration system is now ready for use. No network protocol or zone movement code was touched.
