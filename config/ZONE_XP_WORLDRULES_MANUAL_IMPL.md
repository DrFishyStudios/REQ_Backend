# ZoneServer WorldRules XP Integration - Manual Implementation Guide

## Status: 75% Complete ?

### ? Already Completed

1. **Header file updated** (`REQ_ZoneServer/include/req/zone/ZoneServer.h`)
   - Added `worldRules_` member variable
   - Updated constructor signature to accept `WorldRules` parameter
   - Added `calculateXpReward()` member function declaration

2. **Constructor updated** (`REQ_ZoneServer/src/ZoneServer.cpp`)
   - Added `worldRules` parameter
   - Added initialization: `worldRules_(worldRules)` in initializer list
   - Added WorldRules logging at end of constructor

3. **Main.cpp updated** (`REQ_ZoneServer/src/main.cpp`)
   - Added WorldConfig loading
   - Added WorldRules loading based on ruleset_id
   - Updated ZoneServer construction to pass WorldRules

4. **Anonymous calculateXpReward removed**
   - Old helper function deleted from anonymous namespace

---

## ? Remaining Work (Manual Implementation Required)

### Step 1: Add calculateXpReward Implementation

**Location:** `REQ_ZoneServer/src/ZoneServer.cpp`  
**Position:** Add BEFORE the `processAttack()` function

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

### Step 2: Update processAttack to Use New XP System

**Location:** `REQ_ZoneServer/src/ZoneServer.cpp`  
**Function:** `processAttack()`  
**Section:** After `targetDied = true` block

**Find this code:**
```cpp
if (target.currentHp <= 0) {
    target.currentHp = 0;
    target.isAlive = false;
    targetDied = true;
    
    req::shared::logInfo("zone", std::string{"[COMBAT] NPC slain: npcId="} +
        std::to_string(target.npcId) + ", name=\"" + target.name + 
        "\", killerCharId=" + std::to_string(attacker.characterId));
}
```

**ADD THIS BLOCK IMMEDIATELY AFTER:**

```cpp
// Award XP if target died
if (targetDied) {
    // Calculate XP using WorldRules
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
```

**Update message builder to include XP:**

**Find this code:**
```cpp
// Build result message
std::ostringstream msgBuilder;
if (targetDied) {
    msgBuilder << "You hit " << target.name << " for " << totalDamage 
               << " points of damage. " << target.name << " has been slain!";
} else {
    msgBuilder << "You hit " << target.name << " for " << totalDamage << " points of damage";
}
```

**REPLACE WITH:**
```cpp
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

## Verification Steps

### 1. Build
```bash
cd C:\C++\REQ_Backend
# Build in Visual Studio or command line
```

**Expected:** Clean build with no errors

### 2. Start Servers
```bash
cd x64\Debug
.\REQ_LoginServer.exe
.\REQ_WorldServer.exe
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7780 --zone_name="East Freeport"
```

### 3. Check ZoneServer Logs
```
[INFO] [zone] WorldRules attached to ZoneServer:
[INFO] [zone]   rulesetId=classic_plus_qol
[INFO] [zone]   xp.baseRate=1.000000
[INFO] [zone]   xp.groupBonusPerMember=0.100000
[INFO] [zone]   hotZones=1
```

### 4. Kill an NPC
Use TestClient or future UE client to attack NPC 1001

**Expected Logs:**
```
[INFO] [zone] [COMBAT] NPC slain: npcId=1001, name="A Decaying Skeleton", killerCharId=1
[INFO] [zone] [COMBAT] XP awarded: characterId=1, xpGain=10, newXP=10
[INFO] [zone] [COMBAT] AttackResult: ... msg="You hit A Decaying Skeleton for 15 points of damage. A Decaying Skeleton has been slain! (+10 XP)"
```

### 5. Test XP Modifiers

**Test 1: Change base_rate to 2.0**
Edit `config/world_rules_classic_plus_qol.json`:
```json
"xp": {
  "base_rate": 2.0,
  ...
}
```
Restart ZoneServer, kill NPC ? Expect 20 XP instead of 10

**Test 2: Add hot zone**
Edit `config/world_rules_classic_plus_qol.json`:
```json
"hot_zones": [
  {
    "zone_id": 10,  // East Freeport
    "xp_multiplier": 1.5,
    "loot_multiplier": 1.0,
    "start_date": null,
    "end_date": null
  }
]
```
Restart ZoneServer, kill NPC ? Expect 15 XP (10 * 1.5) at base_rate=1.0

---

## Testing Matrix

| Scenario | base_rate | hot_zone | mob_level | player_level | Expected XP |
|----------|-----------|----------|-----------|--------------|-------------|
| Baseline | 1.0 | none | 1 | 1 | 10 |
| Double XP | 2.0 | none | 1 | 1 | 20 |
| Hot zone | 1.0 | 1.5x | 1 | 1 | 15 |
| Both | 2.0 | 1.5x | 1 | 1 | 30 |
| Red con | 1.0 | none | 3 | 1 | 45 (10*3*1.5) |
| Trivial | 1.0 | none | 1 | 5 | 2 (10*0.25) |

---

## Troubleshooting

### Build Errors

**Error:** `'worldRules_' : undeclared identifier`
- **Fix:** Ensure header file was updated with `req::shared::WorldRules worldRules_;` member

**Error:** `cannot convert from 'const req::shared::ZoneConfig' to 'const req::shared::WorldRules'`
- **Fix:** Check constructor parameter order, should be: `(worldId, zoneId, zoneName, address, port, worldRules, charactersPath)`

### Runtime Errors

**Error:** `Failed to open WorldRules file`
- **Fix:** Verify `config/world_rules_classic_plus_qol.json` exists
- **Fix:** Check that `world_config.json` has correct `ruleset_id`

**Error:** No XP awarded
- **Check:** Is `targetDied` true?
- **Check:** Is `calculateXpReward()` being called?
- **Check:** Is XP >0 (target level >0)?
- **Check:** Add debug log in calculateXpReward to see values

---

## Files to Modify

| File | Section | Action |
|------|---------|--------|
| `REQ_ZoneServer/src/ZoneServer.cpp` | Before `processAttack()` | Add `calculateXpReward()` implementation |
| `REQ_ZoneServer/src/ZoneServer.cpp` | In `processAttack()` after targetDied block | Add XP award logic |
| `REQ_ZoneServer/src/ZoneServer.cpp` | In `processAttack()` message builder | Add "(+XX XP)" to death message |

---

## Summary

? **Header**: Updated  
? **Constructor**: Updated  
? **Main.cpp**: Updated  
? **Old helper**: Removed  
? **calculateXpReward impl**: **ADD THIS** (Step 1)  
? **processAttack XP logic**: **ADD THIS** (Step 2)  
? **processAttack message**: **UPDATE THIS** (Step 2)  

**Once Steps 1 and 2 are complete, XP will be fully integrated with WorldRules!**

---

## Quick Copy-Paste Snippets

### Add calculateXpReward (goes before processAttack)
```cpp
std::uint32_t ZoneServer::calculateXpReward(const ZonePlayer& attacker,
                                            const req::shared::data::ZoneNpc& target) const
{
    if (target.level <= 0) {
        return 0;
    }

    const int attackerLevel = attacker.level;
    const int targetLevel = target.level;
    float baseXp = 10.0f * static_cast<float>(targetLevel);

    int levelDiff = targetLevel - attackerLevel;
    float levelModifier = 1.0f;

    if (levelDiff >= 3) {
        levelModifier = 1.5f;
    } else if (levelDiff >= 1) {
        levelModifier = 1.2f;
    } else if (levelDiff <= -3) {
        levelModifier = 0.25f;
    } else if (levelDiff <= -1) {
        levelModifier = 0.5f;
    }

    float xpRate = worldRules_.xp.baseRate;
    if (xpRate < 0.0f) {
        xpRate = 0.0f;
    }

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

    float groupBonus = 1.0f;
    // TODO: groupBonus = 1.0f + (worldRules_.xp.groupBonusPerMember * (groupSize-1));

    float xpFloat = baseXp * levelModifier * xpRate * hotZoneMult * groupBonus;

    if (xpFloat < 1.0f) {
        xpFloat = 1.0f;
    }

    return static_cast<std::uint32_t>(xpFloat);
}
```

### Add XP award logic (after targetDied block in processAttack)
```cpp
    if (targetDied) {
        std::uint32_t xpReward = calculateXpReward(attacker, target);
        
        if (xpReward > 0) {
            attacker.xp += xpReward;
            attacker.combatStatsDirty = true;
            
            req::shared::logInfo("zone", std::string{"[COMBAT] XP awarded: characterId="} +
                std::to_string(attacker.characterId) + ", xpGain=" + std::to_string(xpReward) +
                ", newXP=" + std::to_string(attacker.xp));
        }
    }
```

### Update message builder (replace existing msgBuilder section)
```cpp
    std::ostringstream msgBuilder;
    if (targetDied) {
        msgBuilder << "You hit " << target.name << " for " << totalDamage 
                   << " points of damage. " << target.name << " has been slain!";
        
        std::uint32_t xpReward = calculateXpReward(attacker, target);
        
        if (xpReward > 0) {
            msgBuilder << " (+" << xpReward << " XP)";
        }
    } else {
        msgBuilder << "You hit " << target.name << " for " << totalDamage << " points of damage";
    }
```

---

**Done! Follow these steps to complete the XP integration.** ?

