# REQ_DataValidator - Quick Reference

## Purpose
Read-only validator for all REQ_Backend config and data files.

## Usage
```bash
cd C:\C++\REQ_Backend
.\x64\Debug\REQ_DataValidator.exe
```

## Exit Codes
- **0** = All validations passed ?
- **1** = Validation failed ?

---

## What It Validates

### 1. Config Files ??
**Files:**
- `config/login_config.json`
- `config/world_config.json`
- `config/worlds.json`
- `config/zones/zone_*_config.json`
- `config/world_rules_<ruleset>.json`

**Checks:**
- ? Valid JSON syntax
- ? Unique world/zone IDs
- ? Valid ports (1-65535)
- ? No duplicate ports
- ? Ruleset ID matches world config

---

### 2. NPC Data ??
**Files:**
- `config/zones/zone_*_npcs.json`

**Checks:**
- ? Contains `"npcs"` array
- ? Unique NPC IDs per file
- ? `npc_id` != 0
- ? `level` > 0
- ? `max_hp` > 0
- ? `aggro_radius` > 0
- ? `leash_radius` > 0

---

### 3. World Rules ??
**Files:**
- `config/world_rules_<ruleset_id>.json`

**Checks:**
- ? Ruleset ID matches world config
- ? All multipliers ? 0:
  - XP rates
  - Loot rates
  - Death XP loss
- ? Hot zones have valid zone IDs
- ? Hot zone multipliers ? 0

---

### 4. Accounts & Characters ??
**Files:**
- `data/accounts/*.json`
- `data/characters/*.json`

**Checks:**
- ? `account_id` != 0
- ? Characters reference existing accounts
- ? Positions are finite (not NaN/Inf)
- ?? `lastWorldId` matches config (warning)
- ?? `lastZoneId` in zone set (warning)

---

## Validation Passes

| Pass | Files | Errors Stop Validation? |
|------|-------|-------------------------|
| **Configs** | All config JSON | ? Yes - blocks deeper checks |
| **NPCs** | Zone NPC files | ? No - continues |
| **Rules** | World rules | ? No - continues |
| **Data** | Accounts/chars | ? No - reports all issues |

---

## Common Errors

### Config Errors
```
[ERROR] Duplicate world_id in worlds.json: 1
[ERROR] Invalid port in worlds.json for world 'Test': 0
[ERROR] Duplicate zone_id across zone config files: 10
```

### NPC Errors
```
[ERROR] NPC with npc_id=0 in zone_10_npcs.json
[ERROR] Duplicate npc_id 101 in file: zone_10_npcs.json
[ERROR] NPC 102 ('gnoll_pup') has invalid level: 0
```

### Rules Errors
```
[ERROR] WorldRules rulesetId mismatch
[ERROR] Negative multiplier in xp.base_rate: -1.5
[ERROR] Hot zone has invalid zone_id=0
```

### Data Errors
```
[ERROR] Account file has account_id=0
[ERROR] Character 42 references unknown accountId 999
[ERROR] Character 43 has non-finite position values
```

---

## Common Warnings

```
[WARN] Accounts directory does not exist (OK for fresh repo)
[WARN] Characters directory does not exist (OK for fresh repo)
[WARN] Character has lastWorldId mismatch (character migrated?)
[WARN] Character has lastZoneId not in zone set (zone removed?)
```

---

## Quick Fixes

### Duplicate Zone ID
```json
// BAD: Two files with same zone_id
zone_10_config.json: { "zone_id": 10, ... }
zone_20_config.json: { "zone_id": 10, ... }  // ? Duplicate!

// FIX: Use unique IDs
zone_20_config.json: { "zone_id": 20, ... }  // ? Unique
```

### Invalid Port
```json
// BAD: Port is 0 or > 65535
{ "port": 0 }     // ? Invalid
{ "port": 99999 } // ? Out of range

// FIX: Use valid port
{ "port": 7778 }  // ? Valid
```

### Orphan Character
```json
// Character references account that doesn't exist
{ "account_id": 999, ... }  // ? Account 999 missing

// FIX: Create account or fix reference
{ "account_id": 1, ... }    // ? Account 1 exists
```

### Non-Finite Position
```json
// Character has NaN/Inf position (corruption)
{ "position_x": "NaN", ... }  // ? Not finite

// FIX: Reset to safe spawn
{ "position_x": 0.0, "position_y": 0.0, "position_z": 0.0 }  // ? Valid
```

---

## Integration

### CI/CD Pipeline
```bash
# Windows batch
.\REQ_DataValidator.exe
if %errorlevel% neq 0 (
    echo Validation failed!
    exit /b 1
)
```

### Pre-Commit Hook
```bash
#!/bin/bash
./REQ_DataValidator.exe || {
    echo "Fix validation errors before committing"
    exit 1
}
```

---

## What It Does NOT Do

- ? **Never creates files**
- ? **Never modifies files**
- ? **Never deletes files**
- ? **Never touches server code**
- ? **Never makes network connections**
- ? **Never starts servers**

---

## Design Principles

1. **Read-Only** - Safe to run on production data
2. **Fast** - Completes in seconds
3. **Clear** - Errors are actionable
4. **Complete** - Shows all issues, not just first
5. **Isolated** - No dependencies on running servers

---

## File Locations

### Config Files
```
config/
??? login_config.json
??? world_config.json
??? worlds.json
??? world_rules_<ruleset>.json
??? zones/
    ??? zone_10_config.json
    ??? zone_10_npcs.json
    ??? zone_20_config.json
    ??? zone_20_npcs.json
```

### Data Files
```
data/
??? accounts/
?   ??? 1.json
?   ??? 2.json
??? characters/
    ??? 1.json
    ??? 2.json
```

---

## Dependencies

### From REQ_Shared
- `req::shared::Logger`
- `req::shared::loadLoginConfig()`
- `req::shared::loadWorldConfig()`
- `req::shared::loadZoneConfig()`
- `req::shared::loadWorldRules()`
- `req::shared::data::Account`
- `req::shared::data::Character`
- `nlohmann::json`

### Standard Library
- `<filesystem>` - File operations
- `<fstream>` - File I/O
- `<cmath>` - `std::isfinite()`

---

## Example Output

### Success ?
```
[INFO] [Validator] Starting REQ data validation...
[INFO] [ConfigValidation] LoginConfig OK: 0.0.0.0:7777
[INFO] [Validator] Validation passed for: Config files
[INFO] [Validator] Validation passed for: NPC data
[INFO] [Validator] Validation passed for: World rules
[INFO] [Validator] Validation passed for: Accounts & characters
[INFO] [Validator] All validation checks passed.
[INFO] [Main] REQ_DataValidator completed successfully.
```

### Failure ?
```
[INFO] [Validator] Starting REQ data validation...
[ERROR] [ConfigValidation] Duplicate zone_id: 10
[ERROR] [NpcValidation] NPC with npc_id=0
[ERROR] [Validator] Validation failed for: Config files
[ERROR] [Validator] Validation failed for: NPC data
[INFO] [Validator] Validation passed for: World rules
[INFO] [Validator] Validation passed for: Accounts & characters
[ERROR] [Validator] Validation finished with 2 failing pass(es).
[ERROR] [Main] REQ_DataValidator completed with errors. Failing passes: 2
```

---

## Summary

| Component | Status |
|-----------|--------|
| Config Validation | ? Implemented |
| NPC Validation | ? Implemented |
| Rules Validation | ? Implemented |
| Data Validation | ? Implemented |
| Read-Only Safety | ? Guaranteed |
| Exit Codes | ? Correct |
| Error Reporting | ? Clear |

**Ready for:** CI/CD integration, pre-commit hooks, manual validation
