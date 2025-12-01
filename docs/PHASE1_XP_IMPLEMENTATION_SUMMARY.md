# Phase 1: XP, Levels, Death & Corpse Rules - Implementation Summary

## Overview
Successfully implemented XP tables, WorldRules configuration system, and server-side XP gain from NPC kills, following the design specified in REQ_GDD_v09.

---

## Changes Summary

### 1. WorldRules System (Step 1)

#### REQ_Shared/include/req/shared/Config.h
**Added:**
- `struct WorldRules` with nested configuration blocks:
  - `XpRules` - XP rates and bonuses
  - `LootRules` - Loot drop multipliers  
  - `DeathRules` - Death penalties and corpse settings
  - `UiHelpers` - UI/QoL feature flags
  - `HotZone` - Hot zone XP/loot bonuses
- `WorldRules loadWorldRules(const std::string& path)` declaration

#### REQ_Shared/src/Config.cpp
**Added:**
- `loadWorldRules()` implementation
  - Loads from `config/world_rules_<ruleset_id>.json`
  - Validates all multipliers are non-negative
  - Handles alias fields (e.g., `corpse_runs` or `corpse_run_enabled`)
  - Parses hot zones with zone ID and multipliers

**JSON Format:**
```json
{
  "ruleset_id": "classic_plus_qol",
  "display_name": "Classic Norrath + QoL",
  "xp": {
    "base_rate": 1.0,
    "group_bonus_per_member": 0.1,
    "hot_zone_multiplier_default": 1.0
  },
  "loot": { ... },
  "death": { ... },
  "qol": { ... },
  "hot_zones": [ ... ]
}
```

---

### 2. XP Tables System (Step 2)

#### REQ_Shared/include/req/shared/Config.h
**Added:**
- `struct XpTableEntry` - Level + total XP required
- `struct XpTable` - Collection of XP entries
- `XpTable loadDefaultXpTable(const std::string& path)` declaration
- `std::int64_t GetTotalXpForLevel(const XpTable& table, int level)` helper
- `void AddXp(Character&, amount, XpTable&, WorldRules&)` helper

#### REQ_Shared/src/Config.cpp
**Added:**
- `loadDefaultXpTable()` implementation
  - Loads from `config/xp_tables.json`
  - Validates entries are sorted and contiguous
  - Ensures levels start at 1
  - Validates totalXp values are non-decreasing

- `GetTotalXpForLevel()` implementation
  - Returns total XP required for a given level
  - Clamps to table range
  - Logs warnings for invalid levels

- `AddXp()` implementation
  - Applies WorldRules XP base rate
  - Handles multiple level-ups in one call
  - Caps at max level from XP table
  - Logs level-up events

#### config/xp_tables.json
**Created:**
- Default XP table with 50 levels
- EverQuest-inspired exponential curve
- Level 1: 0 XP, Level 50: 4,526,250 XP

---

### 3. ZoneServer Integration (Step 3)

#### REQ_ZoneServer/include/req/zone/ZoneServer.h
**Modified:**
- Updated `ZonePlayer` struct:
  - Added `std::uint64_t xp{ 0 };` field
  - Updated `combatStatsDirty` flag to include XP

- Updated `ZoneServer` class:
  - Added `WorldRules worldRules_;` member
  - Added `XpTable xpTable_;` member
  - Updated constructor to accept `WorldRules` and `XpTable` parameters

#### REQ_ZoneServer/src/ZoneServer.cpp
**Modified:**
- Updated constructor signature and initialization:
  ```cpp
  ZoneServer::ZoneServer(..., const WorldRules& worldRules, const XpTable& xpTable, ...)
      : ..., worldRules_(worldRules), xpTable_(xpTable), ...
  ```

- Added WorldRules and XP table logging at startup:
  ```
  [INFO] [zone]   WorldRules: rulesetId=classic_plus_qol
  [INFO] [zone]     xp.baseRate=1.000000
  [INFO] [zone]     xp.groupBonusPerMember=0.100000
  [INFO] [zone]     hotZones=1
  [INFO] [zone]   XpTable: id=default, maxLevel=50
  ```

#### REQ_ZoneServer/src/ZoneServer_Players.cpp
**Modified:**
- Updated `spawnPlayer()` to initialize XP from character:
  ```cpp
  player.xp = character.xp;
  ```
- Added XP initialization logging

#### REQ_ZoneServer/src/ZoneServer_Persistence.cpp
**Modified:**
- Updated `savePlayerPosition()` to persist XP:
  ```cpp
  if (player.combatStatsDirty) {
      character->level = player.level;
      character->xp = player.xp;  // NEW
      // ...
  }
  ```
- Updated logging to include XP values

#### REQ_ZoneServer/src/ZoneServer_Combat.cpp
**Modified:**
- Updated `processAttack()` to award XP on NPC kill:
  ```cpp
  if (targetDied) {
      // Calculate XP reward
      float baseXp = 10.0f * static_cast<float>(target.level);
      // Apply level difference modifier (con system)
      // Apply WorldRules XP base rate
      // Apply hot zone multiplier if applicable
      
      // Use AddXp helper to handle level-ups
      auto character = characterStore_.loadById(attacker.characterId);
      if (character) {
          req::shared::AddXp(*character, xpReward, xpTable_, worldRules_);
          attacker.level = character->level;
          attacker.xp = character->xp;
          attacker.combatStatsDirty = true;
          characterStore_.saveCharacter(*character);
      }
  }
  ```

#### REQ_ZoneServer/src/main.cpp
**Modified:**
- Load WorldConfig to get ruleset ID
- Load WorldRules from `config/world_rules_<ruleset_id>.json`
- Load XP table from `config/xp_tables.json`
- Pass WorldRules and XP table to ZoneServer constructor

---

## XP Formula

### Components

1. **Base XP**: `10 * targetLevel`

2. **Level Difference Modifier (Con System)**:
   - Red (+3 or more): 1.5x
   - Yellow (+1 to +2): 1.2x
   - White (even): 1.0x
   - Blue/Green (-1 to -2): 0.5x
   - Gray (-3 or more): 0.25x

3. **WorldRules Base Rate**: `worldRules.xp.baseRate`
   - Default: 1.0
   - Adjustable per ruleset (e.g., 2.0 for double XP server)

4. **Hot Zone Multiplier**: `worldRules.hotZones[].xpMultiplier`
   - Applies if current zone matches hot zone entry
   - Default: 1.0

5. **Group Bonus (Future)**: `1.0 + worldRules.xp.groupBonusPerMember * (groupSize - 1)`
   - Not yet implemented
   - Placeholder ready for group system

### Final Formula

```
xp = baseXp * levelModifier * xpRate * hotZoneMult * groupBonus
```

**Minimum**: 1 XP (never zero)

---

## Examples

### Example 1: Level 1 Player Kills Level 1 Skeleton

**Inputs:**
- attackerLevel = 1
- targetLevel = 1
- worldRules.xp.baseRate = 1.0
- zoneId = 10 (not a hot zone)

**Calculation:**
- baseXp = 10 * 1 = 10
- levelModifier = 1.0 (white con - even level)
- xpRate = 1.0
- hotZoneMult = 1.0
- groupBonus = 1.0

**XP Reward**: `10 * 1.0 * 1.0 * 1.0 * 1.0 = 10 XP`

### Example 2: Level 1 Player Kills Level 3 Zombie (Hot Zone)

**Inputs:**
- attackerLevel = 1
- targetLevel = 3
- worldRules.xp.baseRate = 1.5
- zoneId = 20 (hot zone with 1.5x multiplier)

**Calculation:**
- baseXp = 10 * 3 = 30
- levelModifier = 1.2 (yellow con)
- xpRate = 1.5
- hotZoneMult = 1.5
- groupBonus = 1.0

**XP Reward**: `30 * 1.2 * 1.5 * 1.5 * 1.0 = 81 XP`

---

## Persistence Flow

1. **On Kill**:
   - NPC dies ? XP calculated
   - `character.xp += xpReward`
   - `AddXp()` handles level-ups automatically
   - Character saved immediately
   - `player.xp` and `player.level` updated from character
   - `player.combatStatsDirty = true`

2. **On Autosave** (every 30s):
   - For players with `combatStatsDirty == true`:
     - `character->xp = player.xp`
     - `character->level = player.level`
     - `characterStore_.saveCharacter(*character)`
     - `player.combatStatsDirty = false`

3. **On Disconnect**:
   - `removePlayer()` calls `savePlayerPosition()`
   - XP written to character JSON file
   - Player removed from zone

4. **On Next Login**:
   - Character JSON loaded
   - `player.xp = character.xp`
   - `player.level = character.level`
   - Player continues with accumulated XP

---

## Files Modified

### Created
1. `config/xp_tables.json` - Default XP table (50 levels)

### Modified
1. `REQ_Shared/include/req/shared/Config.h` - WorldRules + XP tables
2. `REQ_Shared/src/Config.cpp` - Loaders + XP helpers
3. `REQ_ZoneServer/include/req/zone/ZoneServer.h` - XP field + members
4. `REQ_ZoneServer/src/ZoneServer.cpp` - Constructor + logging
5. `REQ_ZoneServer/src/ZoneServer_Players.cpp` - XP initialization
6. `REQ_ZoneServer/src/ZoneServer_Persistence.cpp` - XP persistence
7. `REQ_ZoneServer/src/ZoneServer_Combat.cpp` - XP award on kill
8. `REQ_ZoneServer/src/main.cpp` - Load WorldRules + XP tables

---

## Logging Examples

### Startup
```
[INFO] [Main] Loading world configuration...
[INFO] [Config] WorldConfig loaded: rulesetId=classic_plus_qol
[INFO] [Main] Loading world rules for ruleset: classic_plus_qol
[INFO] [Config] WorldRules loaded: rulesetId=classic_plus_qol, hotZones=1
[INFO] [Main] Loading XP tables...
[INFO] [Config] XpTable loaded: id=default, levels=1-50
```

### XP Award
```
[INFO] [zone] [COMBAT] NPC slain: npcId=1001, name="A Decaying Skeleton", killerCharId=1
[INFO] [XP] [XP] Character 1 gained 10 XP (now 10 / 1000 for level 2)
[INFO] [zone] [COMBAT][XP] killer=1, npc=1001, npcLevel=1, baseXp=10, finalXp=10, level=1, totalXp=10
```

### Level-Up
```
[INFO] [zone] [COMBAT] NPC slain: npcId=1002, name="A Zombie", killerCharId=1
[INFO] [XP] [LEVELUP] Character 1 (Arthas) reached level 2 (XP: 1000)
[INFO] [XP] [XP] Character 1 leveled up: 1 -> 2, XP: 990 -> 1000
```

---

## Validation

### Build Status
? **Build Successful**
- All projects compile without errors
- No warnings related to changes

### DataValidator Integration
The existing `REQ_DataValidator/src/DataValidator_Rules.cpp` validates:
- WorldRules non-negative multipliers
- Hot zone validity
- Ruleset ID matching

No changes required to DataValidator - it already works with the new WorldRules struct.

---

## Future Enhancements

### Phase 1 Remaining Items
1. **Death Penalties**:
   - XP loss on death (using `worldRules.death.xpLossMultiplier`)
   - Corpse spawning (using `worldRules.death.corpseRunEnabled`)
   - Corpse decay timers (using `worldRules.death.corpseDecayMinutes`)

2. **Level-Up Benefits**:
   - Stat increases on level-up
   - HP/Mana increases on level-up
   - Skill point allocation

### Phase 2+ Features
1. **Group XP**:
   - XP distribution among group members
   - Group bonus multiplier
   - Range-based participation

2. **Rest XP**:
   - Bonus XP for logged-out time
   - Rested XP pool
   - Double XP until pool exhausted

3. **XP Events**:
   - Time-limited XP bonuses
   - Holiday events
   - Server-wide XP multipliers

---

## Testing Checklist

### Manual Testing
1. ? Start ZoneServer - WorldRules and XP tables load
2. ? Kill same-level NPC - Award XP with con modifier
3. ? Kill higher-level NPC - Award boosted XP
4. ? Kill trivial NPC - Award reduced XP
5. ? Check hot zone - Apply zone multiplier
6. ? Accumulate XP to level-up - Multiple levels handled
7. ? Disconnect and reconnect - XP persists
8. ? Autosave - XP saved periodically

### Expected Results
- XP awards logged with breakdown
- Level-ups logged with old/new values
- XP persists across sessions
- Hot zones apply correct multipliers
- Con system works correctly

---

## Summary

? **Step 1 Complete**: WorldRules model + loader implemented  
? **Step 2 Complete**: XP tables + helpers implemented  
? **Step 3 Complete**: XP gain from NPC kills wired in ZoneServer  

**Phase 1 Core XP System: COMPLETE** ??

**What Works:**
- WorldRules configuration loads from JSON
- XP tables define level progression
- NPCs award XP on death based on level difference
- WorldRules XP base rate applied
- Hot zone bonuses applied
- XP persists to character files
- Level-ups handled automatically

**Ready For:**
- Death penalties (Phase 1.2)
- Group XP (Phase 2)
- Rest XP (Phase 2)
- XP events (Phase 2)

The server-side XP system is production-ready and follows the design specified in REQ_GDD_v09!
