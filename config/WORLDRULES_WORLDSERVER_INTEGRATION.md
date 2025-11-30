# WorldRules Integration into WorldServer - Implementation Summary

## Overview
WorldServer now loads and attaches WorldRules configuration based on the `ruleset_id` field in `world_config.json`. The rules are loaded at startup and logged for verification, but are not yet used in runtime logic.

---

## Files Modified

### 1. REQ_WorldServer/include/req/world/WorldServer.h

**Constructor Signature Updated:**
```cpp
// Before:
explicit WorldServer(const req::shared::WorldConfig& config,
                    const std::string& charactersPath = "data/characters");

// After:
explicit WorldServer(const req::shared::WorldConfig& config,
                    const req::shared::WorldRules& worldRules,
                    const std::string& charactersPath = "data/characters");
```

**New Member Variable:**
```cpp
req::shared::WorldConfig config_{};
req::shared::WorldRules worldRules_{};  // NEW
```

---

### 2. REQ_WorldServer/src/WorldServer.cpp

**Constructor Updated:**
```cpp
WorldServer::WorldServer(const req::shared::WorldConfig& config,
                         const req::shared::WorldRules& worldRules,
                         const std::string& charactersPath)
    : acceptor_(ioContext_), 
      config_(config), 
      worldRules_(worldRules),  // NEW - Initialize worldRules_
      characterStore_(charactersPath),
      accountStore_("data/accounts") {
```

**Added WorldRules Logging:**
After the existing WorldServer construction logs, the following rules summary is logged:

```cpp
// Log WorldRules details
req::shared::logInfo("world", "WorldRules attached: rulesetId=" + worldRules_.rulesetId +
    ", displayName=" + worldRules_.displayName);
req::shared::logInfo("world", "Rules: XP baseRate=" + std::to_string(worldRules_.xp.baseRate) +
    ", groupBonusPerMember=" + std::to_string(worldRules_.xp.groupBonusPerMember));
req::shared::logInfo("world", "Rules: lootDropMult=" + std::to_string(worldRules_.loot.dropRateMultiplier) +
    ", rareDropMult=" + std::to_string(worldRules_.loot.rareDropMultiplier));
req::shared::logInfo("world", "Rules: deathXpLossMult=" + std::to_string(worldRules_.death.xpLossMultiplier) +
    ", corpseRunEnabled=" + std::string(worldRules_.death.corpseRunEnabled ? "true" : "false"));
req::shared::logInfo("world", "Rules: uiHelpers minimap=" +
    std::string(worldRules_.uiHelpers.minimapEnabled ? "true" : "false") +
    ", questTracker=" + std::string(worldRules_.uiHelpers.questTrackerEnabled ? "true" : "false"));
req::shared::logInfo("world", "Rules: hotZones=" + std::to_string(worldRules_.hotZones.size()));
```

---

### 3. REQ_WorldServer/src/main.cpp

**WorldRules Loading Added:**
After loading `WorldConfig`, the following code was added:

```cpp
// Load world rules based on ruleset_id from config
const std::string rulesetId = config.rulesetId;
std::string worldRulesPath = "config/world_rules_" + rulesetId + ".json";

req::shared::logInfo("Main", std::string{"Loading world rules from: "} + worldRulesPath);
auto worldRules = req::shared::loadWorldRules(worldRulesPath);
```

**WorldServer Construction Updated:**
```cpp
// Before:
req::world::WorldServer server(config, charactersPath);

// After:
req::world::WorldServer server(config, worldRules, charactersPath);
```

---

## Behavior

### Startup Sequence

1. **Load WorldConfig** from `config/world_config.json`
   - Extract `ruleset_id` field (e.g., `"classic_plus_qol"`)

2. **Build WorldRules Path**
   - Construct path: `config/world_rules_<ruleset_id>.json`
   - Example: `config/world_rules_classic_plus_qol.json`

3. **Load WorldRules**
   - Use `req::shared::loadWorldRules(path)`
   - If file is missing or malformed, **startup fails** (exception thrown)

4. **Create WorldServer**
   - Pass both `config` and `worldRules` to constructor
   - WorldServer stores rules in `worldRules_` member

5. **Log Rules Summary**
   - Logs are emitted during WorldServer construction
   - Shows key rule values for verification

---

## Example Logs

### Successful Startup
```
[INFO] [Main] Loading world configuration from: config/world_config.json
[INFO] [Config] Loading WorldConfig from: config/world_config.json
[INFO] [Config] WorldConfig loaded: worldId=1, worldName=CazicThule, address=0.0.0.0, port=7778, rulesetId=classic_plus_qol, zones=3, autoLaunchZones=true
[INFO] [Main] Configuration loaded successfully:
[INFO] [Main]   worldId=1
[INFO] [Main]   worldName=CazicThule
[INFO] [Main]   address=0.0.0.0:7778
[INFO] [Main]   rulesetId=classic_plus_qol
[INFO] [Main]   zones=3
[INFO] [Main]   autoLaunchZones=true
[INFO] [Main] Loading world rules from: config/world_rules_classic_plus_qol.json
[INFO] [Config] Loading WorldRules from: config/world_rules_classic_plus_qol.json
[INFO] [Config] WorldRules loaded: rulesetId=classic_plus_qol, displayName=Classic Norrath + QoL, hotZones=1
[INFO] [Main] Using characters path: data/characters
[INFO] [Main] Configuring SessionService with file: data/sessions.json
[INFO] [world] WorldServer constructed:
[INFO] [world]   worldId=1
[INFO] [world]   worldName=CazicThule
[INFO] [world]   autoLaunchZones=true
[INFO] [world]   zones.size()=3
[INFO] [world]   charactersPath=data/characters
[INFO] [world] WorldRules attached: rulesetId=classic_plus_qol, displayName=Classic Norrath + QoL
[INFO] [world] Rules: XP baseRate=1.000000, groupBonusPerMember=0.100000
[INFO] [world] Rules: lootDropMult=1.000000, rareDropMult=1.000000
[INFO] [world] Rules: deathXpLossMult=1.000000, corpseRunEnabled=true
[INFO] [world] Rules: uiHelpers minimap=true, questTracker=true
[INFO] [world] Rules: hotZones=1
```

### Error: Rules File Missing
```
[INFO] [Main] Loading world rules from: config/world_rules_invalid_ruleset.json
[ERROR] [Config] Failed to open WorldRules file: config/world_rules_invalid_ruleset.json
[ERROR] [Main] Fatal exception: Failed to open WorldRules file: config/world_rules_invalid_ruleset.json
[ERROR] [Main] WorldServer cannot start. Check configuration and try again.
```

### Error: Rules File Malformed
```
[INFO] [Main] Loading world rules from: config/world_rules_classic_plus_qol.json
[ERROR] [Config] Failed to parse WorldRules JSON from config/world_rules_classic_plus_qol.json: [json.exception.parse_error.101] parse error at line 5, column 3: syntax error while parsing object key - invalid literal; last read: '{'
[ERROR] [Main] Fatal exception: Failed to parse WorldRules JSON from config/world_rules_classic_plus_qol.json: ...
[ERROR] [Main] WorldServer cannot start. Check configuration and try again.
```

---

## Configuration Files

### config/world_config.json
```json
{
  "world_id": 1,
  "world_name": "CazicThule",
  "address": "0.0.0.0",
  "port": 7778,
  "ruleset_id": "classic_plus_qol",  // ? Used to determine rules file
  "auto_launch_zones": true,
  "zones": [ ... ]
}
```

### config/world_rules_classic_plus_qol.json
```json
{
  "ruleset_id": "classic_plus_qol",
  "display_name": "Classic Norrath + QoL",
  "description": "Classic EverQuest-inspired pacing ...",
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
    "faction_color_pulses_enabled": true,
    "minimap_enabled": true,
    "quest_tracker_enabled": true,
    "corpse_arrow_enabled": true
  },
  "hot_zones": [ ... ]
}
```

---

## What Was NOT Changed

? **No network protocol changes**
- `ProtocolSchemas.h/cpp` untouched
- `MessageTypes.h` untouched
- No new message types added

? **No ZoneServer changes**
- ZoneServer movement logic unchanged
- ZoneServer tick logic unchanged
- ZoneServer snapshot broadcasting unchanged

? **No client protocol changes**
- No changes to wire formats
- No changes to handshake flow
- No changes to client expectations

? **No runtime behavior changes**
- WorldRules are loaded but not yet used
- XP/loot/death mechanics unchanged
- Hot zones not yet applied

---

## Next Steps (Future Work)

### 1. Pass WorldRules to ZoneServers
- Update ZoneServer constructor to accept `WorldRules`
- Include rules in auto-launch arguments OR
- Load rules in ZoneServer based on worldId/zoneId

### 2. Apply XP Rules
- Use `worldRules_.xp.baseRate` in XP calculations
- Apply `worldRules_.xp.groupBonusPerMember` for group bonuses
- Check hot zones for XP multipliers

### 3. Apply Loot Rules
- Use `worldRules_.loot.dropRateMultiplier` for loot drops
- Apply `worldRules_.loot.coinRateMultiplier` to coin drops
- Use `worldRules_.loot.rareDropMultiplier` for rare items

### 4. Apply Death Rules
- Calculate XP loss using `worldRules_.death.xpLossMultiplier`
- Enforce corpse run mechanics based on `worldRules_.death.corpseRunEnabled`
- Set corpse decay timer to `worldRules_.death.corpseDecayMinutes`

### 5. Enable UI Helpers
- Send UI feature flags to clients during zone handoff
- Clients enable/disable features based on ruleset

### 6. Hot Zone System
- Check player's current zone against `worldRules_.hotZones`
- Apply multipliers if current date is within `startDate` and `endDate`
- Broadcast hot zone status to clients

---

## Verification

### Build Status
? **Build Successful** - No compilation errors

### Testing Checklist
- [x] WorldServer starts successfully
- [x] WorldRules are loaded from correct file path
- [x] Rules are logged at startup
- [x] Missing rules file causes clean error and exit
- [x] Malformed rules JSON causes clean error and exit
- [ ] WorldRules values are used in gameplay (not yet implemented)

### Manual Test
```bash
cd x64/Debug
.\REQ_WorldServer.exe
```

**Expected Output:**
1. WorldConfig loads successfully
2. `ruleset_id` extracted from config
3. WorldRules file path constructed
4. WorldRules loaded successfully
5. Rules summary logged during WorldServer construction
6. Server starts normally

---

## Summary

**What Changed:**
1. WorldServer constructor accepts `WorldRules` parameter
2. WorldServer stores rules in `worldRules_` member variable
3. `main.cpp` loads rules based on `config.rulesetId`
4. Rules summary is logged at startup

**What Works:**
- WorldServer starts with rules attached
- Rules are logged for verification
- Missing/invalid rules file causes clean startup failure

**What Doesn't Work Yet:**
- Rules are not applied to gameplay
- ZoneServers don't receive rules
- XP/loot/death mechanics unchanged

**Status:** ? Integration Complete
**Build:** ? Successful
**Ready For:** Runtime rule application in gameplay systems

---

## Implementation Notes

### Error Handling
- If rules file is missing ? **Fatal error, server won't start**
- If rules file is malformed ? **Fatal error, server won't start**
- This ensures the server always runs with valid, complete rules

### Design Decisions

**Why pass rules to constructor instead of loading inside WorldServer?**
- Keeps WorldServer focused on server logic, not config loading
- Makes rules dependency explicit in API
- Easier to test with different rule sets

**Why fail startup if rules are missing?**
- Running without rules could cause unexpected behavior
- Better to fail fast than run with incorrect/default rules
- Admin gets immediate feedback about configuration issues

**Why not send rules to ZoneServers yet?**
- Task scope limited to WorldServer integration
- ZoneServer integration is next logical step
- Allows testing of loading logic independently

---

## Files Changed Summary

| File | Lines Changed | Type |
|------|---------------|------|
| `REQ_WorldServer/include/req/world/WorldServer.h` | ~5 | Modified (constructor + member) |
| `REQ_WorldServer/src/WorldServer.cpp` | ~15 | Modified (constructor + logging) |
| `REQ_WorldServer/src/main.cpp` | ~8 | Modified (loading + passing rules) |

**Total:** ~28 lines changed across 3 files

---

## Complete! ??

WorldServer now loads and attaches WorldRules at startup. The rules are ready to be used for gameplay logic, but no runtime behavior has been changed yet.
