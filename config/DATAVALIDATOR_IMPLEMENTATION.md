# REQ_DataValidator - Implementation Summary

## Overview
**REQ_DataValidator** is a standalone read-only console executable that validates all game data and configuration files. It ensures config files are well-formed, NPCs are properly defined, world rules are consistent, and accounts/characters reference valid data.

---

## Purpose

### What It Does
- ? **Validates all config files** (login, world, zones, world rules)
- ? **Checks NPC data** for correctness and consistency
- ? **Validates world rules** multipliers and hot zones
- ? **Cross-validates accounts and characters** for referential integrity
- ? **Reports clear errors and warnings** via logging
- ? **Returns appropriate exit codes** for CI/CD integration

### What It Does NOT Do
- ? **Never creates, modifies, or deletes** any files
- ? **Never touches server code** (Login/World/Zone/Chat/TestClient)
- ? **Never modifies protocol** or movement code
- ? **Never starts servers** or makes network connections

---

## Project Structure

```
REQ_DataValidator/
??? include/
?   ??? req/
?       ??? datavalidator/
?           ??? DataValidator.h          # Public interface
??? src/
?   ??? DataValidator.cpp                # Implementation
?   ??? main.cpp                         # Entry point
```

---

## Files Created

### 1. DataValidator.h
**Location:** `REQ_DataValidator/include/req/datavalidator/DataValidator.h`

**Purpose:** Public API for validation system

**Contents:**
```cpp
struct ValidationResult {
    bool success{true};
    std::size_t errorCount{0};
    std::size_t warningCount{0};
};

ValidationResult runAllValidations(
    const std::string& configRoot = "config",
    const std::string& accountsRoot = "data/accounts",
    const std::string& charactersRoot = "data/characters");
```

**Usage:**
- Simple interface with sensible defaults
- Returns structured result for programmatic handling
- Exit code friendly (0 on success, 1 on failure)

---

### 2. DataValidator.cpp
**Location:** `REQ_DataValidator/src/DataValidator.cpp`

**Purpose:** Core validation logic

**Key Components:**

#### Main Orchestrator: `runAllValidations()`
Runs all validation passes and accumulates results:

```cpp
accumulate(validateConfigs(...), "Config files");
accumulate(validateNpcData(...), "NPC data");
accumulate(validateWorldRules(...), "World rules");
accumulate(validateAccountsAndCharacters(...), "Accounts & characters");
```

#### Internal Validation Passes

##### 1. `validateConfigs()`
**Validates:**
- ? `config/login_config.json` - Login server config
- ? `config/world_config.json` - World server config
- ? `config/worlds.json` - World list for LoginServer
- ? `config/zones/zone_*_config.json` - Individual zone configs

**Checks:**
- Login/world ports are valid (1-65535)
- No duplicate world IDs or ports in `worlds.json`
- Zone IDs are unique across zone config files
- Zone ports in `world_config.json` are valid and unique
- All required fields present
- Config files parse as valid JSON

**Example Errors:**
```
[ERROR] [ConfigValidation] Duplicate world_id in worlds.json: 1
[ERROR] [ConfigValidation] Invalid port in worlds.json for world 'TestWorld': 0
[ERROR] [ConfigValidation] Duplicate zone_id across zone config files: 10
```

##### 2. `validateNpcData()`
**Validates:**
- ? `config/zones/zone_*_npcs.json` - NPC definitions per zone

**Checks:**
- NPC file contains `"npcs"` array
- Each NPC has unique `npc_id` within file
- `npc_id` is not 0
- `level` > 0
- `max_hp` > 0
- `aggro_radius` > 0
- `leash_radius` > 0
- Valid JSON structure

**Example Errors:**
```
[ERROR] [NpcValidation] NPC with npc_id=0 in config/zones/zone_10_npcs.json
[ERROR] [NpcValidation] Duplicate npc_id 101 in file: config/zones/zone_10_npcs.json
[ERROR] [NpcValidation] NPC 102 ('a_gnoll_pup') has invalid level: 0
[ERROR] [NpcValidation] NPC 103 ('orc_centurion') has invalid max_hp: -50
```

##### 3. `validateWorldRules()`
**Validates:**
- ? `config/world_rules_<ruleset_id>.json` - World rules for active ruleset

**Checks:**
- Rules file exists for `worldConfig.rulesetId`
- `ruleset_id` in rules file matches `world_config.json`
- All XP multipliers are non-negative:
  - `xp.base_rate` ? 0
  - `xp.group_bonus_per_member` ? 0
  - `xp.hot_zone_multiplier_default` ? 0
- All loot multipliers are non-negative:
  - `loot.drop_rate_multiplier` ? 0
  - `loot.coin_rate_multiplier` ? 0
  - `loot.rare_drop_multiplier` ? 0
- Death rules are valid:
  - `death.xp_loss_multiplier` ? 0
- Hot zones are valid:
  - `zone_id` is not 0
  - `xp_multiplier` ? 0
  - `loot_multiplier` ? 0

**Example Errors:**
```
[ERROR] [WorldRulesValidation] WorldRules rulesetId 'classic' does not match worldConfig.rulesetId 'classic_plus_qol'
[ERROR] [WorldRulesValidation] Negative multiplier in WorldRules for xp.base_rate: -1.5
[ERROR] [WorldRulesValidation] Hot zone has invalid zone_id=0
```

##### 4. `validateAccountsAndCharacters()`
**Validates:**
- ? `data/accounts/*.json` - Account files
- ? `data/characters/*.json` - Character files
- ? Cross-references between accounts and characters

**Checks:**

**Accounts:**
- `account_id` is not 0
- Valid JSON structure
- Files parse correctly

**Characters:**
- `character_id` is valid
- `account_id` references an existing account (if accounts exist)
- `last_world_id` matches configured `world_id` (warning if mismatch)
- `last_zone_id` is in configured zone set (warning if mismatch)
- Position values (`position_x`, `position_y`, `position_z`) are finite (not NaN/Inf)
- Valid JSON structure

**Example Errors:**
```
[ERROR] [AccountValidation] Account file data/accounts/1.json has account_id=0
[ERROR] [CharacterValidation] Character 42 ('TestChar') references unknown accountId 999
[ERROR] [CharacterValidation] Character 43 has non-finite position values (x,y,z)
```

**Example Warnings:**
```
[WARN] [CharacterValidation] Character 42 has lastWorldId=2 which does not match configured worldId=1
[WARN] [CharacterValidation] Character 43 has lastZoneId=99 which is not in configured zone set
```

---

### 3. main.cpp
**Location:** `REQ_DataValidator/src/main.cpp`

**Purpose:** Entry point for executable

**Behavior:**
1. Initialize logging: `initLogger("REQ_DataValidator")`
2. Run all validations with default paths
3. Log final result
4. Return exit code:
   - **0** if all validations pass
   - **1** if any validation fails

**Example Output (Success):**
```
[INFO] [Validator] Starting REQ data validation...
[INFO] [Validator]   configRoot    = config
[INFO] [Validator]   accountsRoot  = data/accounts
[INFO] [Validator]   charactersRoot= data/characters
[INFO] [ConfigValidation] LoginConfig OK: 0.0.0.0:7777
[INFO] [NpcValidation] Validating NPC data file: zone_10_npcs.json
[INFO] [WorldRulesValidation] Loading WorldRules from: config/world_rules_classic_plus_qol.json
[INFO] [Validator] Validation passed for: Config files
[INFO] [Validator] Validation passed for: NPC data
[INFO] [Validator] Validation passed for: World rules
[INFO] [Validator] Validation passed for: Accounts & characters
[INFO] [Validator] All validation checks passed.
[INFO] [Main] REQ_DataValidator completed successfully.
```

**Example Output (Failure):**
```
[INFO] [Validator] Starting REQ data validation...
[ERROR] [ConfigValidation] Duplicate world_id in worlds.json: 1
[ERROR] [NpcValidation] NPC with npc_id=0 in config/zones/zone_10_npcs.json
[ERROR] [Validator] Validation failed for: Config files
[ERROR] [Validator] Validation failed for: NPC data
[INFO] [Validator] Validation passed for: World rules
[INFO] [Validator] Validation passed for: Accounts & characters
[ERROR] [Validator] Validation finished with 2 failing pass(es).
[ERROR] [Main] REQ_DataValidator completed with errors. Failing passes: 2
```

---

## Dependencies

### Shared Components (REQ_Shared)
**Reused Code:**
- `req::shared::Logger` - Logging infrastructure
- `req::shared::Config` - Config loaders:
  - `loadLoginConfig()`
  - `loadWorldConfig()`
  - `loadZoneConfig()`
  - `loadWorldListConfig()`
  - `loadWorldRules()`
- `req::shared::DataModels` - Data structures:
  - `data::Account`
  - `data::Character`
- `nlohmann::json` - JSON parsing

**External Libraries:**
- `<filesystem>` (C++17) - File system operations
- `<fstream>` - File I/O
- `<cmath>` - `std::isfinite()` for position validation

---

## Validation Pass Summary

| Pass | Files Validated | Key Checks | Failure Mode |
|------|----------------|------------|--------------|
| **Config files** | `config/*.json`, `config/zones/*.json` | Valid JSON, unique IDs, valid ports, no duplicates | Hard error (blocks other checks) |
| **NPC data** | `config/zones/zone_*_npcs.json` | Unique NPC IDs, valid stats, positive values | Hard error |
| **World rules** | `config/world_rules_<ruleset>.json` | Non-negative multipliers, valid hot zones, matching ruleset ID | Hard error |
| **Accounts & Characters** | `data/accounts/*.json`, `data/characters/*.json` | Referential integrity, finite positions, valid IDs | Hard error for integrity issues, warnings for mismatches |

---

## Error vs Warning Policy

### Hard Errors (Fail Validation)
- Missing required files (except data directories - see warnings)
- Invalid JSON syntax
- Invalid data values (negative stats, zero IDs, etc.)
- Duplicate IDs (worlds, zones, NPCs)
- Duplicate ports
- Account/character referential integrity violations
- Non-finite position values (NaN/Inf)

### Warnings (Don't Fail Validation)
- Missing data directories (`data/accounts`, `data/characters`)
  - *Rationale: Fresh repo may not have any data yet*
- `lastWorldId` mismatch
  - *Rationale: Character may have been imported from another world*
- `lastZoneId` not in configured zone set
  - *Rationale: Zone config may have changed after character was created*
- Missing `worlds.json`
  - *Rationale: Optional file for LoginServer world list*

---

## Usage Examples

### Basic Usage
```bash
cd C:\C++\REQ_Backend
.\x64\Debug\REQ_DataValidator.exe
```

### CI/CD Integration
```bash
# In build pipeline
.\REQ_DataValidator.exe
if %errorlevel% neq 0 (
    echo Validation failed, aborting deployment
    exit /b 1
)
echo Validation passed, continuing deployment
```

### Pre-Commit Hook
```bash
#!/bin/bash
# .git/hooks/pre-commit
./REQ_DataValidator.exe || {
    echo "Data validation failed. Fix errors before committing."
    exit 1
}
```

---

## Exit Codes

| Code | Meaning | When It Happens |
|------|---------|-----------------|
| **0** | Success | All validation passes succeeded |
| **1** | Failure | One or more validation passes failed OR unhandled exception |

---

## Design Decisions

### Why Read-Only?
- **Safety:** Never risk corrupting production data
- **Trust:** Can run on live data without fear
- **Simplicity:** No state management or rollback logic needed

### Why Not Auto-Fix?
- **Transparency:** User sees exactly what's wrong
- **Control:** User decides how to fix issues
- **Complexity:** Auto-fixing would require heuristics and could introduce bugs

### Why Separate Executable?
- **Isolation:** No risk of affecting running servers
- **Deployment:** Can run on any machine with the data
- **Flexibility:** Can be run standalone, in CI/CD, or pre-commit

### Why Accumulate All Errors?
- **Efficiency:** See all problems in one run
- **Better UX:** Fix multiple issues at once
- **CI/CD Friendly:** Single validation run shows all problems

---

## Future Enhancements

### Command-Line Arguments
```bash
REQ_DataValidator.exe --config=custom/config --accounts=custom/accounts --verbose
```

**Proposed Flags:**
- `--config=<path>` - Override config root
- `--accounts=<path>` - Override accounts root
- `--characters=<path>` - Override characters root
- `--verbose` - Show all warnings (not just errors)
- `--json` - Output results as JSON
- `--fix` - Auto-fix common issues (future consideration)

### Additional Validations

#### Loot Tables
- Validate `config/zones/zone_*_loot.json`
- Check item IDs reference valid items
- Ensure drop rates sum to ? 100%

#### Quests
- Validate `config/zones/zone_*_quests.json`
- Check quest prerequisites form valid DAG
- Ensure quest rewards reference valid items/NPCs

#### Spawn Points
- Validate `config/zones/zone_*_spawns.json`
- Check spawn points are within zone bounds
- Ensure spawn timers are positive

#### Cross-Zone References
- Hot zones reference valid zone IDs
- Character bind points reference valid zones
- NPC patrol paths reference valid zones

### Performance Optimizations
- **Parallel Validation:** Run passes concurrently
- **Incremental Validation:** Only re-validate changed files
- **Caching:** Cache parsed configs for multiple runs

---

## Testing

### Manual Test Cases

#### Test 1: Fresh Repo (No Data)
**Setup:** Delete `data/accounts` and `data/characters` directories

**Expected Result:**
- Warnings about missing data directories
- Config and NPC validations still pass
- Exit code 0

#### Test 2: Invalid Config
**Setup:** Make `world_config.json` invalid (e.g., duplicate zone IDs)

**Expected Result:**
- Error logged for config validation
- Exit code 1

#### Test 3: Orphan Character
**Setup:** Character references non-existent account ID

**Expected Result:**
- Error logged for character validation
- Exit code 1

#### Test 4: All Valid
**Setup:** Clean, valid config and data files

**Expected Result:**
- All passes succeed
- Exit code 0

### Automated Test Integration
```cpp
// Future: Unit tests for individual validation functions
TEST(DataValidator, DetectsDuplicateZoneIds) {
    // Create temp config with duplicate zone IDs
    // Run validateConfigs()
    // Assert failure
}

TEST(DataValidator, AllowsMissingDataDir) {
    // Create config without data directory
    // Run validateAccountsAndCharacters()
    // Assert warning but no error
}
```

---

## Integration with Existing Systems

### No Changes Required
- ? **LoginServer** - Not touched
- ? **WorldServer** - Not touched
- ? **ZoneServer** - Not touched
- ? **ChatServer** - Not touched
- ? **TestClient** - Not touched
- ? **Protocol Code** - Not touched
- ? **Movement Code** - Not touched

### New Capabilities Enabled
- **CI/CD Validation:** Fail builds on invalid data
- **Pre-Deployment Checks:** Validate before pushing to production
- **Data Migration Verification:** Ensure migrated data is valid
- **Config Auditing:** Regular validation of production configs

---

## Known Limitations

### Current Limitations

1. **No Cross-File NPC ID Uniqueness**
   - NPCs must be unique within a file
   - Same NPC ID can exist in different zone files
   - *Future:* Add global NPC ID registry check

2. **No Zone Bounds Validation**
   - Doesn't check if NPC positions are within zone geometry
   - Doesn't validate spawn point coordinates
   - *Future:* Add zone geometry bounds checking

3. **No Item Reference Validation**
   - Doesn't validate loot table item IDs
   - Doesn't check quest reward item IDs
   - *Future:* Add item database validation

4. **No Performance Metrics**
   - Doesn't report validation duration
   - Doesn't show progress for large datasets
   - *Future:* Add timing and progress reporting

### Design Trade-offs

**Chosen:** Simple, focused validation of existing data
**Not Chosen:** Comprehensive game logic validation
- *Rationale:* Keep validator simple and fast
- *Future:* Add game logic validation as separate pass

---

## Troubleshooting

### Common Issues

#### Issue: "Failed to open config file"
**Cause:** Config file doesn't exist or has wrong name
**Fix:** Check file path and name match expected pattern

#### Issue: "JSON parse error"
**Cause:** Invalid JSON syntax (missing comma, quote, etc.)
**Fix:** Validate JSON with linter, check for trailing commas

#### Issue: "Duplicate zone_id"
**Cause:** Two zone config files define same zone_id
**Fix:** Ensure each zone has unique ID

#### Issue: "Character references unknown accountId"
**Cause:** Character file has account_id that doesn't exist
**Fix:** Create missing account or fix character's account_id

#### Issue: "Non-finite position values"
**Cause:** Character position has NaN or Inf (corruption)
**Fix:** Reset character position to zone safe spawn

---

## Summary

**Status:** ? **Implementation Complete**

**What Was Created:**
1. `DataValidator.h` - Public interface
2. `DataValidator.cpp` - Core validation logic (4 passes)
3. `main.cpp` - Entry point with exit codes

**What It Validates:**
- ? Config files (login, world, zones, world rules)
- ? NPC data (stats, IDs, uniqueness)
- ? World rules (multipliers, hot zones)
- ? Accounts & characters (integrity, references)

**What It Doesn't Touch:**
- ? No file modifications
- ? No server code changes
- ? No protocol changes
- ? No movement code changes

**Key Features:**
- Read-only operation
- Clear error reporting
- CI/CD friendly exit codes
- Reuses existing shared code
- Handles missing data gracefully

**Next Steps:**
1. Add to Visual Studio solution
2. Test with actual data files
3. Integrate into CI/CD pipeline
4. Add to pre-commit hooks
5. Document in README

---

## Validation Pass Details

### 1. Config Files Validation
**Validates:** Login, World, Zone configs and world list
**Checks:** JSON validity, unique IDs, valid ports
**Exit on Failure:** Yes (blocks deeper validation)

### 2. NPC Data Validation
**Validates:** Zone NPC definitions
**Checks:** Unique NPC IDs, positive stats, valid radii
**Exit on Failure:** No (continues to other passes)

### 3. World Rules Validation
**Validates:** World rules for active ruleset
**Checks:** Non-negative multipliers, matching ruleset ID, valid hot zones
**Exit on Failure:** No (continues to other passes)

### 4. Accounts & Characters Validation
**Validates:** Account/character data and cross-references
**Checks:** Referential integrity, finite positions, valid IDs
**Exit on Failure:** No (reports all issues)

---

## Complete! ??

REQ_DataValidator is ready to ensure data integrity across the entire REQ_Backend ecosystem.
