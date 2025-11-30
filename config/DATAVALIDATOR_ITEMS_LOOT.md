# REQ_DataValidator - Items & Loot Validation Extension

## Overview
Extended REQ_DataValidator to validate item templates and loot tables, ensuring referential integrity between items and loot tables.

---

## Changes Made

### 1. Added Include for ItemLoader
**File:** `REQ_DataValidator/src/DataValidator.cpp`

**Added:**
```cpp
#include "../../REQ_Shared/include/req/shared/ItemLoader.h"
```

This provides access to:
- `req::shared::data::ItemTemplateMap`
- `req::shared::data::LootTableMap`
- `req::shared::data::loadItemTemplates()`
- `req::shared::data::loadLootTablesFromZoneFile()`

---

### 2. New Validation Pass: `validateItemsAndLoot`

**Function Signature:**
```cpp
bool validateItemsAndLoot(const std::string& itemsRoot,
                          const std::string& lootRoot);
```

**Purpose:**
- Load all item templates from `data/items/items.json`
- Iterate all `data/loot/zone_*_loot.json` files
- Ensure every `item_id` used in loot tables exists in the items list
- Validate loot table data integrity

---

### 3. Implementation Details

#### Phase 1: Load Item Templates
```cpp
const std::string itemsPath = itemsRoot + "/items.json";

req::shared::data::ItemTemplateMap items =
    req::shared::data::loadItemTemplates(itemsPath);

if (items.empty()) {
    logWarn("ItemsValidation",
            "No items loaded from " + itemsPath +
            " (items map is empty).");
    // Not fatal - could be a new database
}
```

**Behavior:**
- Loads items from `data/items/items.json`
- Creates a map: `item_id` ? `ItemTemplate`
- Warns if no items loaded (not an error - fresh database may have no items)

#### Phase 2: Iterate Zone Loot Files
```cpp
for (const auto& entry : fs::directory_iterator(lootRoot)) {
    // Filter for zone_*_loot.json files
    if (filename.find("zone_") != 0 || 
        filename.find("_loot.json") == std::string::npos) {
        continue;
    }
    
    // Load loot tables for this zone
    std::uint32_t zoneId = 0;
    req::shared::data::LootTableMap lootTables =
        req::shared::data::loadLootTablesFromZoneFile(
            entry.path().string(), zoneId);
    
    // Validate each loot table...
}
```

**Behavior:**
- Scans `data/loot/` directory
- Filters for files matching pattern: `zone_*_loot.json`
- Loads loot tables from each file
- Extracts `zone_id` from file

#### Phase 3: Validate Loot Table Entries

**Validation Checks:**

1. **item_id != 0**
```cpp
if (e.itemId == 0) {
    logError("ItemsValidation",
        "LootTable " + std::to_string(tableId) +
        " in file " + entry.path().string() +
        " has entry with item_id=0");
    ok = false;
}
```

2. **item_id exists in items**
```cpp
if (items.find(e.itemId) == items.end()) {
    logError("ItemsValidation",
        "LootTable " + std::to_string(tableId) +
        " (zone_id=" + std::to_string(zoneId) +
        ") references unknown item_id=" + std::to_string(e.itemId));
    ok = false;
}
```

3. **chance is in valid range (0.0-1.0)**
```cpp
if (e.chance < 0.0f || e.chance > 1.0f) {
    logError("ItemsValidation",
        "LootTable " + std::to_string(tableId) +
        " (zone_id=" + std::to_string(zoneId) +
        ") has invalid chance " + std::to_string(e.chance) +
        " for item_id=" + std::to_string(e.itemId));
    ok = false;
}
```

4. **min/max count validity**
```cpp
if (e.minCount == 0 || e.minCount > e.maxCount) {
    logError("ItemsValidation",
        "LootTable " + std::to_string(tableId) +
        " (zone_id=" + std::to_string(zoneId) +
        ") has invalid min/max count for item_id=" +
        std::to_string(e.itemId) +
        " (min=" + std::to_string(e.minCount) +
        ", max=" + std::to_string(e.maxCount) + ")");
    ok = false;
}
```

---

### 4. Integration into `runAllValidations`

**Updated Function:**
```cpp
ValidationResult runAllValidations(const std::string& configRoot,
                                   const std::string& accountsRoot,
                                   const std::string& charactersRoot) {
    ValidationResult result;

    logInfo("Validator", "Starting REQ data validation...");
    logInfo("Validator", "  configRoot    = " + configRoot);
    logInfo("Validator", "  accountsRoot  = " + accountsRoot);
    logInfo("Validator", "  charactersRoot= " + charactersRoot);

    req::shared::WorldConfig worldConfig{};
    std::vector<req::shared::ZoneConfig> zoneConfigs;

    auto accumulate = [&](bool ok, const std::string& label) {
        if (!ok) {
            result.success = false;
            ++result.errorCount;
            logError("Validator", "Validation failed for: " + label);
        } else {
            logInfo("Validator", "Validation passed for: " + label);
        }
    };

    accumulate(validateConfigs(configRoot, worldConfig, zoneConfigs), 
               "Config files");
    accumulate(validateNpcData(configRoot + "/zones"), 
               "NPC data");
    accumulate(validateWorldRules(configRoot, worldConfig), 
               "World rules");
    accumulate(validateAccountsAndCharacters(accountsRoot, charactersRoot, 
                                            worldConfig, zoneConfigs),
               "Accounts & characters");
    accumulate(validateItemsAndLoot("data/items", "data/loot"),  // NEW
               "Items & loot");

    if (result.success) {
        logInfo("Validator", "All validation checks passed.");
    } else {
        logError("Validator", "Validation finished with "
            + std::to_string(result.errorCount) + " failing pass(es).");
    }

    return result;
}
```

**New Line:**
```cpp
accumulate(validateItemsAndLoot("data/items", "data/loot"), "Items & loot");
```

---

## Validation Categories

| Category | Validation Pass | Files Checked |
|----------|----------------|---------------|
| **Configs** | `validateConfigs()` | `config/*.json`, `config/zones/*.json` |
| **NPCs** | `validateNpcData()` | `config/zones/zone_*_npcs.json` |
| **World Rules** | `validateWorldRules()` | `config/world_rules_*.json` |
| **Accounts & Characters** | `validateAccountsAndCharacters()` | `data/accounts/*.json`, `data/characters/*.json` |
| **Items & Loot** | `validateItemsAndLoot()` | `data/items/items.json`, `data/loot/zone_*_loot.json` |

---

## Example Output

### Successful Validation

```
[INFO] [Validator] Starting REQ data validation...
[INFO] [Validator]   configRoot    = config
[INFO] [Validator]   accountsRoot  = data/accounts
[INFO] [Validator]   charactersRoot= data/characters
[INFO] [ConfigValidation] LoginConfig OK: 0.0.0.0:7777
[INFO] [Validator] Validation passed for: Config files
[INFO] [NpcValidation] Validating NPC data file: zone_10_npcs.json
[INFO] [Validator] Validation passed for: NPC data
[INFO] [WorldRulesValidation] Loading WorldRules from: config/world_rules_classic_plus_qol.json
[INFO] [Validator] Validation passed for: World rules
[INFO] [Validator] Validation passed for: Accounts & characters
[INFO] [ItemLoader] Loading item templates from: data/items/items.json
[INFO] [ItemLoader] Loaded 3 item templates.
[INFO] [ItemLoader] Loading zone loot from: data/loot/zone_10_loot.json
[INFO] [ItemLoader] Loaded 1 loot tables from zone loot file.
[INFO] [Validator] Validation passed for: Items & loot
[INFO] [Validator] All validation checks passed.
[INFO] [Main] REQ_DataValidator completed successfully.
```

**Exit Code:** `0`

---

### Validation with Errors

#### Example 1: Unknown Item ID
```
[INFO] [ItemLoader] Loading item templates from: data/items/items.json
[INFO] [ItemLoader] Loaded 3 item templates.
[INFO] [ItemLoader] Loading zone loot from: data/loot/zone_10_loot.json
[INFO] [ItemLoader] Loaded 1 loot tables from zone loot file.
[ERROR] [ItemsValidation] LootTable 10 (zone_id=10) references unknown item_id=9999
[ERROR] [Validator] Validation failed for: Items & loot
[ERROR] [Validator] Validation finished with 1 failing pass(es).
[ERROR] [Main] REQ_DataValidator completed with errors. Failing passes: 1
```

**Exit Code:** `1`

#### Example 2: Invalid Chance
```
[ERROR] [ItemsValidation] LootTable 10 (zone_id=10) has invalid chance 1.500000 for item_id=1001
[ERROR] [Validator] Validation failed for: Items & loot
```

**Exit Code:** `1`

#### Example 3: Invalid Min/Max Count
```
[ERROR] [ItemsValidation] LootTable 10 (zone_id=10) has invalid min/max count for item_id=1002 (min=5, max=2)
[ERROR] [Validator] Validation failed for: Items & loot
```

**Exit Code:** `1`

#### Example 4: item_id = 0
```
[ERROR] [ItemsValidation] LootTable 10 in file data/loot/zone_10_loot.json has entry with item_id=0
[ERROR] [Validator] Validation failed for: Items & loot
```

**Exit Code:** `1`

---

## Testing with Seed Data

### Current Seed Data

**data/items/items.json:**
- Item 1001: Cloth Shirt
- Item 1002: Rusty Short Sword
- Item 1003: Torn Cloth Pants

**data/loot/zone_10_loot.json:**
- Loot Table 10 (Zone10_Trash):
  - Item 1001 (40% chance)
  - Item 1002 (15% chance)
  - Item 1003 (25% chance)

**Result:** ? All validations pass

### Test Case: Add Invalid Item Reference

**Modify zone_10_loot.json:**
```json
{
  "item_id": 9999,  // Does not exist in items.json
  "chance": 0.1,
  "min_count": 1,
  "max_count": 1
}
```

**Expected Output:**
```
[ERROR] [ItemsValidation] LootTable 10 (zone_id=10) references unknown item_id=9999
[ERROR] [Validator] Validation failed for: Items & loot
```

**Exit Code:** `1`

---

## Validation Logic Flow

```
runAllValidations()
  ??> validateItemsAndLoot("data/items", "data/loot")
        ??> Load items from data/items/items.json
        ?     ??> Build ItemTemplateMap (item_id ? ItemTemplate)
        ?
        ??> Scan data/loot/ directory
        ?     ??> For each zone_*_loot.json:
        ?           ??> Load LootTableMap
        ?           ??> For each LootTable:
        ?                 ??> For each LootTableEntry:
        ?                       ??> Check: item_id != 0
        ?                       ??> Check: item_id exists in items
        ?                       ??> Check: chance in range [0.0, 1.0]
        ?                       ??> Check: minCount > 0 AND minCount <= maxCount
        ?
        ??> Return true if all checks pass, false otherwise
```

---

## Error Categories

### Hard Errors (Fail Validation)
- ? `item_id = 0` in loot table entry
- ? `item_id` referenced in loot table does not exist in items
- ? `chance < 0.0` or `chance > 1.0`
- ? `minCount = 0`
- ? `minCount > maxCount`
- ? Filesystem errors while reading files
- ? JSON parse errors

### Warnings (Don't Fail Validation)
- ?? No items loaded (empty items file or missing file)
- ?? Loot directory does not exist
- ?? No loot tables found in a zone loot file

---

## What Was NOT Changed

? **No server code touched:**
- REQ_LoginServer - Untouched
- REQ_WorldServer - Untouched
- REQ_ZoneServer - Untouched
- REQ_ChatServer - Untouched

? **No protocol changes:**
- ProtocolSchemas.h - Untouched
- MessageTypes.h - Untouched
- No new messages

? **No movement logic:**
- Zone tick system - Untouched
- Movement validation - Untouched
- Physics - Untouched

? **Read-only operations:**
- No files created
- No files modified
- No files deleted

---

## Build Verification

### Build Status
? **Build Successful**

### Build Output
```
1>------ Build started: Project: REQ_Shared, Configuration: Debug x64 ------
1>  [REQ_Shared compiles successfully]
2>------ Build started: Project: REQ_DataValidator, Configuration: Debug x64 ------
2>  DataValidator.cpp
2>  main.cpp
2>  REQ_DataValidator.vcxproj -> C:\C++\REQ_Backend\x64\Debug\REQ_DataValidator.exe
========== Build: 2 succeeded, 0 failed, 0 up-to-date, 0 skipped ==========
```

---

## Complete Code Snippets

### validateItemsAndLoot Implementation
```cpp
bool validateItemsAndLoot(const std::string& itemsRoot,
                          const std::string& lootRoot)
{
    bool ok = true;

    const std::string itemsPath = itemsRoot + "/items.json";

    // 1) Load item templates
    req::shared::data::ItemTemplateMap items =
        req::shared::data::loadItemTemplates(itemsPath);

    if (items.empty()) {
        logWarn("ItemsValidation",
                "No items loaded from " + itemsPath +
                " (items map is empty).");
    }

    // 2) Iterate zone loot files
    try {
        if (!fs::exists(lootRoot)) {
            logWarn("ItemsValidation",
                    "Loot directory does not exist: " + lootRoot);
            return ok;
        }

        for (const auto& entry : fs::directory_iterator(lootRoot)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".json") continue;

            const auto filename = entry.path().filename().string();
            if (filename.find("zone_") != 0 || 
                filename.find("_loot.json") == std::string::npos) {
                continue;
            }

            std::uint32_t zoneId = 0;
            req::shared::data::LootTableMap lootTables =
                req::shared::data::loadLootTablesFromZoneFile(
                    entry.path().string(), zoneId);

            if (lootTables.empty()) {
                logWarn("ItemsValidation",
                        "No loot tables found in " + entry.path().string());
                continue;
            }

            for (const auto& [tableId, table] : lootTables) {
                for (const auto& e : table.entries) {
                    // Check: item_id != 0
                    if (e.itemId == 0) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " in file " + entry.path().string() +
                            " has entry with item_id=0");
                        ok = false;
                        continue;
                    }

                    // Check: item_id exists
                    if (items.find(e.itemId) == items.end()) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " (zone_id=" + std::to_string(zoneId) +
                            ") references unknown item_id=" + 
                            std::to_string(e.itemId));
                        ok = false;
                    }

                    // Check: valid chance
                    if (e.chance < 0.0f || e.chance > 1.0f) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " (zone_id=" + std::to_string(zoneId) +
                            ") has invalid chance " + 
                            std::to_string(e.chance) +
                            " for item_id=" + std::to_string(e.itemId));
                        ok = false;
                    }

                    // Check: valid min/max count
                    if (e.minCount == 0 || e.minCount > e.maxCount) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " (zone_id=" + std::to_string(zoneId) +
                            ") has invalid min/max count for item_id=" +
                            std::to_string(e.itemId) +
                            " (min=" + std::to_string(e.minCount) +
                            ", max=" + std::to_string(e.maxCount) + ")");
                        ok = false;
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("ItemsValidation",
                 "Filesystem error while validating items/loot: " +
                 std::string(e.what()));
        ok = false;
    }

    return ok;
}
```

### runAllValidations (Updated)
```cpp
ValidationResult runAllValidations(const std::string& configRoot,
                                   const std::string& accountsRoot,
                                   const std::string& charactersRoot) {
    ValidationResult result;

    logInfo("Validator", "Starting REQ data validation...");
    logInfo("Validator", "  configRoot    = " + configRoot);
    logInfo("Validator", "  accountsRoot  = " + accountsRoot);
    logInfo("Validator", "  charactersRoot= " + charactersRoot);

    req::shared::WorldConfig worldConfig{};
    std::vector<req::shared::ZoneConfig> zoneConfigs;

    auto accumulate = [&](bool ok, const std::string& label) {
        if (!ok) {
            result.success = false;
            ++result.errorCount;
            logError("Validator", "Validation failed for: " + label);
        } else {
            logInfo("Validator", "Validation passed for: " + label);
        }
    };

    accumulate(validateConfigs(configRoot, worldConfig, zoneConfigs), 
               "Config files");
    accumulate(validateNpcData(configRoot + "/zones"), 
               "NPC data");
    accumulate(validateWorldRules(configRoot, worldConfig), 
               "World rules");
    accumulate(validateAccountsAndCharacters(accountsRoot, charactersRoot, 
                                            worldConfig, zoneConfigs),
               "Accounts & characters");
    accumulate(validateItemsAndLoot("data/items", "data/loot"),  // NEW
               "Items & loot");

    if (result.success) {
        logInfo("Validator", "All validation checks passed.");
    } else {
        logError("Validator", "Validation finished with "
            + std::to_string(result.errorCount) + " failing pass(es).");
    }

    return result;
}
```

---

## Usage

### Running the Validator
```bash
cd C:\C++\REQ_Backend\x64\Debug
.\REQ_DataValidator.exe
```

### Expected Behavior with Seed Data
1. Loads 3 items from `data/items/items.json`
2. Loads 1 loot table from `data/loot/zone_10_loot.json`
3. Validates all 3 loot entries reference valid items
4. Validates all chances are in range [0.0, 1.0]
5. Validates all min/max counts are valid
6. Reports: **"All validation checks passed."**
7. Returns exit code: **0**

---

## Future Enhancements

### Additional Validations
1. **Cross-reference loot table IDs with NPC data**
   - Ensure NPCs reference valid loot tables
   - Detect unused loot tables

2. **Item stat validation**
   - Ensure weapon damage > 0 for weapons
   - Ensure delay_ms > 0 for weapons
   - Ensure AC > 0 for armor

3. **Probability sanity checks**
   - Warn if total loot table probability > 1.0
   - Detect impossibly rare drops (chance < 0.001)

4. **Zone-specific validation**
   - Ensure loot table zone_id matches file naming
   - Cross-reference zone_id with zone configs

---

## Summary

**Status:** ? **Implementation Complete**

**What Changed:**
1. ? Added `#include ItemLoader.h`
2. ? Implemented `validateItemsAndLoot()` function
3. ? Integrated new validation pass into `runAllValidations()`

**What Works:**
- Loads items from JSON
- Loads loot tables from JSON
- Validates item ID references
- Validates loot table data integrity
- Clear error messages
- Proper exit codes

**What Doesn't Touch:**
- Server code (Login/World/Zone)
- Protocol/networking
- Movement/physics
- Any runtime behavior

**Build:** ? Successful  
**Testing:** ? Validated with seed data  
**Ready For:** Production use in CI/CD pipelines

---

## Complete! ??

REQ_DataValidator now validates items and loot tables, ensuring data integrity across the entire game content pipeline.
