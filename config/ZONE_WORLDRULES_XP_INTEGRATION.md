# ZoneServer WorldRules Integration - Implementation Summary

## Summary

ZoneServer now loads WorldRules at startup and uses them for XP calculation, including base rate modifiers and hot zone bonuses.

---

## Changes Made

### 1. Updated ZoneServer.h

**Added worldRules_ member:**
```cpp
// Zone configuration
ZoneConfig zoneConfig_{};

// World rules (for XP calculation, loot rates, etc.)
req::shared::WorldRules worldRules_{};

// Character persistence
req::shared::CharacterStore characterStore_;
```

**Updated constructor signature:**
```cpp
ZoneServer(std::uint32_t worldId,
           std::uint32_t zoneId,
           const std::string& zoneName,
           const std::string& address,
           std::uint16_t port,
           const req::shared::WorldRules& worldRules,  // NEW
           const std::string& charactersPath = "data/characters");
```

**Added calculateXpReward member function:**
```cpp
// XP calculation using WorldRules
std::uint32_t calculateXpReward(const ZonePlayer& attacker,
                                const req::shared::data::ZoneNpc& target) const;
```

---

### 2. Updated ZoneServer.cpp Constructor

**Updated signature and initializer list:**
```cpp
ZoneServer::ZoneServer(std::uint32_t worldId,
                       std::uint32_t zoneId,
                       const std::string& zoneName,
                       const std::string& address,
                       std::uint16_t port,
                       const req::shared::WorldRules& worldRules,  // NEW
                       const std::string& charactersPath)
    : acceptor_(ioContext_), tickTimer_(ioContext_), autosaveTimer_(ioContext_),
      worldId_(worldId), zoneId_(zoneId), zoneName_(zoneName), 
      address_(address), port_(port),
      worldRules_(worldRules),  // NEW - Initialize worldRules_
      characterStore_(charactersPath) {
```

**Added logging at end of constructor:**
```cpp
// Log WorldRules attachment
req::shared::logInfo("zone", "WorldRules attached to ZoneServer:");
req::shared::logInfo("zone", "  rulesetId=" + worldRules_.rulesetId);
req::shared::logInfo("zone", "  xp.baseRate=" + std::to_string(worldRules_.xp.baseRate));
req::shared::logInfo("zone", "  xp.groupBonusPerMember=" + std::to_string(worldRules_.xp.groupBonusPerMember));
req::shared::logInfo("zone", "  hotZones=" + std::to_string(worldRules_.hotZones.size()));
```

---

### 3. Removed Anonymous calculateXpReward Helper

**Deleted old helper from anonymous namespace:**
```cpp
namespace {
    // Simulation constants
    constexpr float TICK_RATE_HZ = 20.0f;
    // ...
    
    // OLD: Removed this helper
    // std::uint32_t calculateXpReward(int attackerLevel, int targetLevel) { ... }
}
```

---

### 4. Implemented calculateXpReward as Member Function

**Added to ZoneServer.cpp:**
```cpp
std::uint32_t ZoneServer::calculateXpReward(const ZonePlayer& attacker,
                                            const req::shared::data::ZoneNpc& target) const
{
    // If target has no meaningful level, skip
    if (target.level <= 0) {
        return 0;
    }

    const int attackerLevel = attacker.level;
    const int targetLevel = target.level;

    // 1) Base XP from target level
    float baseXp = 10.0f * static_cast<float>(targetLevel);

    // 2) Level difference modifier (con-based)
    int levelDiff = targetLevel - attackerLevel;
    float levelModifier = 1.0f;

    if (levelDiff >= 3) {
        levelModifier = 1.5f;   // red con
    } else if (levelDiff >= 1) {
        levelModifier = 1.2f;   // yellow
    } else if (levelDiff <= -3) {
        levelModifier = 0.25f;  // gray (trivial)
    } else if (levelDiff <= -1) {
        levelModifier = 0.5f;   // blue/green
    }
    // else: even level, modifier = 1.0f (white con)

    // 3) WorldRules XP base rate
    float xpRate = worldRules_.xp.baseRate;
    if (xpRate < 0.0f) {
        xpRate = 0.0f;
    }

    // 4) Hot zone multiplier (optional)
    float hotZoneMult = 1.0f;
    const std::uint32_t zoneId = zoneConfig_.zoneId;

    for (const auto& hz : worldRules_.hotZones) {
        if (hz.zoneId == zoneId) {
            if (hz.xpMultiplier > 0.0f) {
                hotZoneMult = hz.xpMultiplier;
            }
            break;
        }
    }

    // 5) Group bonus (TODO for future)
    float groupBonus = 1.0f;
    // TODO: when group system exists, multiply by (1.0f + worldRules_.xp.groupBonusPerMember * (groupSize-1));

    float xpFloat = baseXp * levelModifier * xpRate * hotZoneMult * groupBonus;

    if (xpFloat < 1.0f) {
        xpFloat = 1.0f;
    }

    return static_cast<std::uint32_t>(xpFloat);
}
```

---

### 5. Updated processAttack to Use New Member Function

**Changed call signature in processAttack:**
```cpp
// OLD:
// std::uint32_t xpReward = calculateXpReward(attacker.level, target.level);

// NEW:
std::uint32_t xpReward = calculateXpReward(attacker, target);
```

**Full context in processAttack:**
```cpp
if (targetDied) {
    // Compute XP using member function
    std::uint32_t xpReward = calculateXpReward(attacker, target);
    
    if (xpReward > 0) {
        // Apply XP to zone player state
        attacker.xp += xpReward;
        
        // Mark combat stats dirty so XP gets persisted on autosave/disconnect
        attacker.combatStatsDirty = true;
        
        req::shared::logInfo("zone", std::string{"[COMBAT] XP awarded: characterId="} +
            std::to_string(attacker.characterId) + ", xpGain=" + std::to_string(xpReward) +
            ", newXP=" + std::to_string(attacker.xp));
    }
}

// Build result message
std::ostringstream msgBuilder;
if (targetDied) {
    msgBuilder << "You hit " << target.name << " for " << totalDamage 
               << " points of damage. " << target.name << " has been slain!";
    
    // Append XP info to message if XP was awarded
    std::uint32_t xpReward = calculateXpReward(attacker, target);
    
    if (xpReward > 0) {
        msgBuilder << " (+" << xpReward << " XP)";
    }
} else {
    msgBuilder << "You hit " << target.name << " for " << totalDamage << " points of damage";
}
```

---

### 6. Updated ZoneServer main.cpp

**Added WorldRules loading before ZoneServer construction:**
```cpp
// Load world config to discover ruleset_id
const std::string worldConfigPath = "config/world_config.json";
auto worldConfig = req::shared::loadWorldConfig(worldConfigPath);

const std::string rulesetId = worldConfig.rulesetId;
std::string worldRulesPath = "config/world_rules_" + rulesetId + ".json";

req::shared::logInfo("ZoneMain",
    std::string{"Loading world rules for ZoneServer from: "} + worldRulesPath);

auto worldRules = req::shared::loadWorldRules(worldRulesPath);

// Optional: Verify worldId match
if (worldConfig.worldId != worldId) {
    req::shared::logWarn("ZoneMain",
        std::string{"World ID mismatch: worldConfig="} + std::to_string(worldConfig.worldId) +
        ", server=" + std::to_string(worldId));
}
```

**Updated ZoneServer construction:**
```cpp
// OLD:
// req::zone::ZoneServer server(worldId, zoneId, zoneName, address, port, charactersPath);

// NEW:
req::zone::ZoneServer server(worldId, zoneId, zoneName, address, port, worldRules, charactersPath);
```

---

## XP Formula Breakdown

### Components

1. **Base XP:** `10 * targetLevel`
2. **Level Difference Modifier (Con):**
   - Red (+3 or more): 1.5x
   - Yellow (+1 to +2): 1.2x
   - White (even): 1.0x
   - Blue/Green (-1 to -2): 0.5x
   - Gray (-3 or more): 0.25x

3. **WorldRules Base Rate:** `worldRules_.xp.baseRate`
   - From `config/world_rules_<ruleset>.json`
   - Default: 1.0
   - Can be adjusted per ruleset (e.g., 2.0 for double XP server)

4. **Hot Zone Multiplier:** `worldRules_.hotZones[].xpMultiplier`
   - If current zone matches a hot zone entry
   - Default: 1.0
   - Example: 1.5x for hot zone

5. **Group Bonus (TODO):** `1.0 + worldRules_.xp.groupBonusPerMember * (groupSize - 1)`
   - Not yet implemented (groups don't exist)
   - Placeholder for future

### Final Formula

```cpp
xp = baseXp * levelModifier * xpRate * hotZoneMult * groupBonus
```

**Minimum:** 1 XP (never zero)

---

## Example Calculations

### Example 1: Level 1 Player Kills Level 1 Skeleton (No Hot Zone)

**Inputs:**
- attackerLevel = 1
- targetLevel = 1
- worldRules.xp.baseRate = 1.0
- zoneId = 10 (not a hot zone)

**Calculation:**
1. baseXp = 10 * 1 = 10
2. levelDiff = 1 - 1 = 0 (white con)
3. levelModifier = 1.0
4. xpRate = 1.0
5. hotZoneMult = 1.0
6. groupBonus = 1.0

**XP Reward:** `10 * 1.0 * 1.0 * 1.0 * 1.0 = 10 XP`

---

### Example 2: Level 1 Player Kills Level 3 Zombie (Hot Zone)

**Inputs:**
- attackerLevel = 1
- targetLevel = 3
- worldRules.xp.baseRate = 1.5 (boosted ruleset)
- zoneId = 20 (hot zone with 1.5x multiplier)

**Calculation:**
1. baseXp = 10 * 3 = 30
2. levelDiff = 3 - 1 = 2 (yellow con)
3. levelModifier = 1.2
4. xpRate = 1.5
5. hotZoneMult = 1.5
6. groupBonus = 1.0

**XP Reward:** `30 * 1.2 * 1.5 * 1.5 * 1.0 = 81 XP`

---

### Example 3: Level 5 Player Kills Level 1 Rat (Trivial)

**Inputs:**
- attackerLevel = 5
- targetLevel = 1
- worldRules.xp.baseRate = 1.0
- zoneId = 10 (not a hot zone)

**Calculation:**
1. baseXp = 10 * 1 = 10
2. levelDiff = 1 - 5 = -4 (gray con - trivial)
3. levelModifier = 0.25
4. xpRate = 1.0
5. hotZoneMult = 1.0
6. groupBonus = 1.0

**XP Reward:** `10 * 0.25 * 1.0 * 1.0 * 1.0 = 2.5 ? 2 XP` (minimum 1 XP enforced)

---

## What Was NOT Changed

? **No movement code touched**
- MovementIntent handling unchanged
- Physics simulation unchanged
- Position validation unchanged

? **No protocol changes**
- `AttackResultData` struct unchanged
- Wire format unchanged
- No new message types

? **No message string format changes**
- Still appends "(+XX XP)" to existing message
- Just the XP value calculation changed

? **No LoginServer/WorldServer logic changes**
- Authentication flow unchanged
- Character list unchanged
- EnterWorld unchanged

---

## Logging Examples

### Startup Logs

```
[INFO] [ZoneMain] Loading world rules for ZoneServer from: config/world_rules_classic_plus_qol.json
[INFO] [Config] Loading WorldRules from: config/world_rules_classic_plus_qol.json
[INFO] [Config] WorldRules loaded: rulesetId=classic_plus_qol, displayName=Classic Norrath + QoL, hotZones=1
[INFO] [zone] ZoneServer constructed:
[INFO] [zone]   worldId=1
[INFO] [zone]   zoneId=10
[INFO] [zone]   zoneName="East Freeport"
[INFO] [zone]   address=0.0.0.0
[INFO] [zone]   port=7779
[INFO] [zone]   charactersPath=data/characters
[INFO] [zone]   tickRate=20 Hz
[INFO] [zone]   moveSpeed=70.000000 uu/s
[INFO] [zone]   broadcastFullState=true
[INFO] [zone]   interestRadius=2000.000000
[INFO] [zone] WorldRules attached to ZoneServer:
[INFO] [zone]   rulesetId=classic_plus_qol
[INFO] [zone]   xp.baseRate=1.000000
[INFO] [zone]   xp.groupBonusPerMember=0.100000
[INFO] [zone]   hotZones=1
```

### XP Award Logs

**Standard Kill:**
```
[INFO] [zone] [COMBAT] NPC slain: npcId=1001, name="A Decaying Skeleton", killerCharId=1
[INFO] [zone] [COMBAT] XP awarded: characterId=1, xpGain=10, newXP=10
```

**Hot Zone Kill (1.5x multiplier):**
```
[INFO] [zone] [COMBAT] NPC slain: npcId=2001, name="A Gnoll Scout", killerCharId=1
[INFO] [zone] [COMBAT] XP awarded: characterId=1, xpGain=54, newXP=64
```
(Calculation: 10 * 3 * 1.2 * 1.0 * 1.5 = 54 XP)

---

## Testing

### Build Status
? **Build Successful**

### Test Cases

1. **Kill same-level mob (white con):**
   - Expected: Base XP * xp.baseRate
   - Example: 10 XP with baseRate=1.0

2. **Kill higher-level mob (red con):**
   - Expected: Base XP * 1.5 * xp.baseRate
   - Example: 45 XP for level 3 mob (10 * 3 * 1.5 * 1.0)

3. **Kill trivial mob (gray con):**
   - Expected: Base XP * 0.25 * xp.baseRate
   - Example: 2 XP for level 1 mob at level 5

4. **Kill mob in hot zone:**
   - Expected: Base XP * levelMod * xp.baseRate * hotZoneMult
   - Example: 54 XP for level 3 mob in 1.5x hot zone

5. **Adjust xp.baseRate in config:**
   - Change `world_rules_classic_plus_qol.json` xp.baseRate to 2.0
   - Restart ZoneServer
   - Expected: All XP rewards doubled

---

## Future Enhancements

### Group XP (TODO)

When group system is implemented:

```cpp
// In calculateXpReward, replace:
// float groupBonus = 1.0f;

// With:
int groupSize = getGroupSize(attacker); // hypothetical
if (groupSize > 1) {
    groupBonus = 1.0f + (worldRules_.xp.groupBonusPerMember * (groupSize - 1));
}
```

**Example:** 3-player group with groupBonusPerMember=0.1:
```
groupBonus = 1.0 + (0.1 * (3 - 1)) = 1.2
```

### Hot Zone Date Validation

Currently hot zones apply unconditionally. Future enhancement:

```cpp
// Check startDate and endDate
if (!hz.startDate.empty() && !hz.endDate.empty()) {
    // Parse dates and check if current date is in range
    if (!isDateInRange(currentDate, hz.startDate, hz.endDate)) {
        continue; // Skip this hot zone
    }
}
```

---

## Files Modified

1. `REQ_ZoneServer/include/req/zone/ZoneServer.h`
   - Added `worldRules_` member
   - Updated constructor signature
   - Added `calculateXpReward` member function declaration

2. `REQ_ZoneServer/src/ZoneServer.cpp`
   - Updated constructor to accept and initialize `worldRules_`
   - Added WorldRules logging
   - Removed anonymous `calculateXpReward` helper
   - Implemented `calculateXpReward` as member function
   - Updated `processAttack` to call member function

3. `REQ_ZoneServer/src/main.cpp`
   - Added WorldConfig and WorldRules loading
   - Updated ZoneServer construction to pass WorldRules

---

## Complete! ?

ZoneServer now:
1. ? Loads WorldRules at startup
2. ? Stores worldRules_ member
3. ? Uses xp.baseRate in XP calculation
4. ? Applies hot zone multipliers
5. ? Leaves room for group bonuses (TODO)
6. ? No movement/protocol changes

**XP is now fully driven by WorldRules configuration!** ??

