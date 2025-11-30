# Items and Loot System - Data-Only Implementation

## Overview
Added data-only support for items and loot tables to REQ_Shared. This is a **backend-only** implementation that does NOT touch any network/protocol or Zone movement logic.

---

## Files Created/Modified

### 1. Modified: REQ_Shared/include/req/shared/DataModels.h
**Purpose:** Extended with item and loot data structures

**New Enums:**
```cpp
enum class ItemSlot : std::uint8_t {
    None = 0, Head, Chest, Legs, Feet, Hands, MainHand, OffHand,
    Finger, Neck, Back, Waist, Wrist, Shoulders, Range, Ammo
};

enum class ItemType : std::uint8_t {
    Misc = 0, Weapon, Armor, Consumable, Quest, Bag
};
```

**New Structures:**
```cpp
struct ItemStats {
    int ac{0};      // Armor class
    int hp{0};      // Hit points bonus
    int mana{0};    // Mana bonus
    
    // EverQuest-style stats
    int str{0}, sta{0}, agi{0}, dex{0};
    int wis{0}, intel{0}, cha{0};
};

struct ItemTemplate {
    std::uint32_t itemId{0};
    std::string name;
    ItemSlot slot{ItemSlot::None};
    ItemType type{ItemType::Misc};
    
    int requiredLevel{0};
    int iconId{0};
    
    std::uint32_t maxStackSize{1};
    std::uint32_t valueCopper{0};
    
    ItemStats stats;
    int damage{0};      // For weapons
    int delayMs{0};     // For weapons (attack speed)
    bool isMagic{false};
};

struct LootTableEntry {
    std::uint32_t itemId{0};
    float chance{0.0f};       // 0.0–1.0 (drop probability)
    std::uint32_t minCount{1};
    std::uint32_t maxCount{1};
};

struct LootTable {
    std::uint32_t lootTableId{0};
    std::string name;
    std::vector<LootTableEntry> entries;
};
```

**JSON Serialization:**
- `ItemStats` - Full bidirectional JSON conversion
- `ItemTemplate` - Full bidirectional JSON conversion with enum helpers
- `LootTableEntry` - Full bidirectional JSON conversion
- `LootTable` - Full bidirectional JSON conversion

**Enum Helpers:**
```cpp
ItemSlot itemSlotFromString(const std::string& s);
std::string itemSlotToString(ItemSlot slot);

ItemType itemTypeFromString(const std::string& s);
std::string itemTypeToString(ItemType type);
```

---

### 2. Created: REQ_Shared/include/req/shared/ItemLoader.h
**Purpose:** Item and loot table loader interface

**Type Aliases:**
```cpp
using ItemTemplateMap = std::unordered_map<std::uint32_t, ItemTemplate>;
using LootTableMap = std::unordered_map<std::uint32_t, LootTable>;
```

**Functions:**
```cpp
// Load all item templates from a JSON file
// Expected format: { "items": [ ... ] }
ItemTemplateMap loadItemTemplates(const std::string& path);

// Load loot tables from a zone-specific loot file
// Expected format: { "zone_id": N, "loot_tables": [ ... ], "npc_loot": [ ... ] }
// Returns the loot tables and sets outZoneId
LootTableMap loadLootTablesFromZoneFile(const std::string& path, std::uint32_t& outZoneId);
```

---

### 3. Created: REQ_Shared/src/ItemLoader.cpp
**Purpose:** Item and loot table loader implementation

**Features:**
- ? Robust error handling with logging
- ? Validates item_id != 0
- ? Detects duplicate item IDs
- ? Validates loot_table_id != 0
- ? Detects duplicate loot table IDs
- ? Handles missing files gracefully (warnings, not errors)
- ? Handles malformed JSON with clear error messages
- ? Uses existing Logger infrastructure

**Error Handling:**
```cpp
// Missing file
if (!fs::exists(path)) {
    logWarn("ItemLoader", "Items file does not exist: " + path);
    return map;
}

// Invalid JSON
catch (const json::exception& e) {
    logError("ItemLoader", "Failed to parse items JSON: " + e.what());
    return map;
}

// Invalid data
if (item.itemId == 0) {
    logError("ItemLoader", "Item with item_id=0 in " + path);
    continue;
}
```

---

### 4. Created: data/items/items.json
**Purpose:** Seed item data

**Contents:**
```json
{
  "items": [
    {
      "item_id": 1001,
      "name": "Cloth Shirt",
      "slot": "Chest",
      "type": "Armor",
      "required_level": 1,
      "icon_id": 1,
      "max_stack_size": 1,
      "value_copper": 5,
      "stats": { "ac": 3, "hp": 0, ... },
      "damage": 0,
      "delay_ms": 0,
      "is_magic": false
    },
    {
      "item_id": 1002,
      "name": "Rusty Short Sword",
      "slot": "MainHand",
      "type": "Weapon",
      "required_level": 1,
      "icon_id": 2,
      "max_stack_size": 1,
      "value_copper": 15,
      "stats": { "ac": 0, ... },
      "damage": 4,
      "delay_ms": 3000,
      "is_magic": false
    },
    {
      "item_id": 1003,
      "name": "Torn Cloth Pants",
      "slot": "Legs",
      "type": "Armor",
      "required_level": 1,
      "icon_id": 3,
      "max_stack_size": 1,
      "value_copper": 4,
      "stats": { "ac": 2, ... },
      "damage": 0,
      "delay_ms": 0,
      "is_magic": false
    }
  ]
}
```

**Items Provided:**
1. **Cloth Shirt** (ID: 1001)
   - Armor (Chest slot)
   - AC: 3
   - Value: 5 copper
   - Level 1 starter gear

2. **Rusty Short Sword** (ID: 1002)
   - Weapon (Main Hand slot)
   - Damage: 4
   - Delay: 3000ms (3 seconds)
   - Value: 15 copper
   - Level 1 starter weapon

3. **Torn Cloth Pants** (ID: 1003)
   - Armor (Legs slot)
   - AC: 2
   - Value: 4 copper
   - Level 1 starter gear

---

### 5. Created: data/loot/zone_10_loot.json
**Purpose:** Seed loot table data for Zone 10

**Contents:**
```json
{
  "zone_id": 10,
  "loot_tables": [
    {
      "loot_table_id": 10,
      "name": "Zone10_Trash",
      "entries": [
        {
          "item_id": 1001,
          "chance": 0.4,
          "min_count": 1,
          "max_count": 1
        },
        {
          "item_id": 1002,
          "chance": 0.15,
          "min_count": 1,
          "max_count": 1
        },
        {
          "item_id": 1003,
          "chance": 0.25,
          "min_count": 1,
          "max_count": 1
        }
      ]
    }
  ],
  "npc_loot": [
    {
      "npc_id": 1,
      "loot_table_id": 10
    },
    {
      "npc_id": 2,
      "loot_table_id": 10
    }
  ]
}
```

**Loot Table: Zone10_Trash (ID: 10)**
| Item ID | Item Name | Drop Chance | Count |
|---------|-----------|-------------|-------|
| 1001 | Cloth Shirt | 40% | 1 |
| 1002 | Rusty Short Sword | 15% | 1 |
| 1003 | Torn Cloth Pants | 25% | 1 |

**Total Drop Chance:** 80% (20% chance of no loot)

**NPC Assignments:**
- NPC ID 1 ? Loot Table 10
- NPC ID 2 ? Loot Table 10

---

## Usage Examples

### Loading Items

```cpp
#include "req/shared/ItemLoader.h"

// Load all item templates
auto items = req::shared::data::loadItemTemplates("data/items/items.json");

// Access a specific item
if (items.count(1001)) {
    const auto& clothShirt = items.at(1001);
    std::cout << "Item: " << clothShirt.name << "\n";
    std::cout << "AC: " << clothShirt.stats.ac << "\n";
    std::cout << "Value: " << clothShirt.valueCopper << " copper\n";
}

// Iterate all items
for (const auto& [itemId, item] : items) {
    std::cout << "Item " << itemId << ": " << item.name << "\n";
}
```

### Loading Loot Tables

```cpp
#include "req/shared/ItemLoader.h"

// Load loot tables for a zone
std::uint32_t zoneId = 0;
auto lootTables = req::shared::data::loadLootTablesFromZoneFile(
    "data/loot/zone_10_loot.json", 
    zoneId
);

std::cout << "Loaded loot for zone: " << zoneId << "\n";

// Access a specific loot table
if (lootTables.count(10)) {
    const auto& table = lootTables.at(10);
    std::cout << "Loot Table: " << table.name << "\n";
    
    for (const auto& entry : table.entries) {
        std::cout << "  Item " << entry.itemId 
                  << " - Chance: " << (entry.chance * 100) << "%\n";
    }
}
```

### Enum Conversions

```cpp
#include "req/shared/DataModels.h"

// String to enum
ItemSlot slot = itemSlotFromString("Chest");
// slot == ItemSlot::Chest

// Enum to string
std::string slotName = itemSlotToString(ItemSlot::MainHand);
// slotName == "MainHand"

// JSON serialization (automatic)
ItemTemplate item;
item.itemId = 1001;
item.name = "Test Item";
item.slot = ItemSlot::Head;
item.type = ItemType::Armor;

nlohmann::json j = item;  // Automatic to_json
std::cout << j.dump(2);   // Pretty print

// JSON deserialization (automatic)
ItemTemplate item2 = j.get<ItemTemplate>();  // Automatic from_json
```

---

## Design Decisions

### Why Separate ItemStats?
- **Extensibility:** Easy to add new stats without changing ItemTemplate
- **Clarity:** Stats are grouped together logically
- **Reusability:** ItemStats can be used elsewhere (buffs, temporary effects)

### Why Float for Loot Chance?
- **Precision:** 0.0–1.0 range allows for precise probabilities
- **Standard:** Common convention in game development
- **Example:** 0.4 = 40% chance, 0.15 = 15% chance

### Why Min/Max Count?
- **Flexibility:** Supports variable loot quantities
- **Example:** "1-3 arrows" ? minCount=1, maxCount=3
- **Current:** All items use 1-1 (single drops)

### Why Separate Loot Files Per Zone?
- **Organization:** Each zone has its own loot configuration
- **Performance:** Load only what's needed
- **Maintainability:** Easier to edit zone-specific loot
- **Modularity:** Zones can be added/removed independently

### Why Include npc_loot in Zone Loot File?
- **Future Use:** ZoneServer will need NPC ? Loot Table mapping
- **Validation:** REQ_DataValidator can cross-check NPC IDs
- **Current:** Not used yet, but ready for implementation

---

## Data Schema Reference

### items.json Schema

```json
{
  "items": [
    {
      "item_id": <uint32>,           // Unique item ID (required, > 0)
      "name": <string>,               // Display name (required)
      "slot": <string>,               // Item slot (see ItemSlot enum)
      "type": <string>,               // Item type (see ItemType enum)
      "required_level": <int>,        // Minimum level to equip
      "icon_id": <int>,               // Icon identifier
      "max_stack_size": <uint32>,     // Max stack (1 for non-stackable)
      "value_copper": <uint32>,       // Vendor value in copper
      "stats": {                      // Item stats (all optional, default 0)
        "ac": <int>,
        "hp": <int>,
        "mana": <int>,
        "str": <int>,
        "sta": <int>,
        "agi": <int>,
        "dex": <int>,
        "wis": <int>,
        "int": <int>,
        "cha": <int>
      },
      "damage": <int>,                // Weapon damage
      "delay_ms": <int>,              // Weapon delay (milliseconds)
      "is_magic": <bool>              // Is this a magic item?
    }
  ]
}
```

### zone_*_loot.json Schema

```json
{
  "zone_id": <uint32>,                // Zone identifier
  "loot_tables": [
    {
      "loot_table_id": <uint32>,      // Unique loot table ID (> 0)
      "name": <string>,                // Loot table name
      "entries": [
        {
          "item_id": <uint32>,         // Item to drop (must exist in items)
          "chance": <float>,           // Drop probability (0.0–1.0)
          "min_count": <uint32>,       // Minimum quantity
          "max_count": <uint32>        // Maximum quantity
        }
      ]
    }
  ],
  "npc_loot": [                        // Future use: NPC ? Loot Table mapping
    {
      "npc_id": <uint32>,              // NPC identifier
      "loot_table_id": <uint32>        // Loot table to use
    }
  ]
}
```

---

## Validation Support

### REQ_DataValidator Integration

The new item and loot data can be validated using REQ_DataValidator (to be implemented):

**Items Validation:**
- ? item_id > 0
- ? No duplicate item_ids
- ? name is not empty
- ? slot is valid enum value
- ? type is valid enum value
- ? max_stack_size > 0
- ? Weapon items have damage > 0 and delay_ms > 0

**Loot Tables Validation:**
- ? loot_table_id > 0
- ? No duplicate loot_table_ids per zone
- ? All item_ids reference valid items
- ? chance values are 0.0–1.0
- ? min_count <= max_count
- ? npc_loot references valid loot_table_ids

---

## What Was NOT Changed

? **No existing structures modified:**
- `Account` - Unchanged
- `Character` - Unchanged
- `PlayerCore` - Unchanged
- `ItemDef` - Unchanged (legacy, not removed)
- `NpcTemplate` - Unchanged
- `ZoneNpc` - Unchanged

? **No network/protocol changes:**
- `ProtocolSchemas.h` - Untouched
- `MessageTypes.h` - Untouched
- No new message types
- No wire format changes

? **No Zone movement changes:**
- ZoneServer movement logic - Untouched
- Physics/tick system - Untouched
- Snapshot broadcasting - Untouched

? **No runtime integration yet:**
- Items not hooked into inventory system
- Loot not hooked into NPC death
- No item spawning/pickup yet
- No loot rolling yet

---

## Future Work

### Phase 1: Inventory System
- Add `std::vector<ItemInstance>` to Character
- ItemInstance = ItemTemplate + quantity + durability
- Save/load inventory to character JSON
- Protocol messages for inventory updates

### Phase 2: Loot Generation
- ZoneServer loads zone loot tables on startup
- On NPC death, roll loot from assigned table
- Create loot corpse entity
- Protocol messages for loot notifications

### Phase 3: Item Pickup/Drop
- Player interaction with loot corpses
- Add item to player inventory
- Remove item from loot corpse
- Protocol messages for pickup/drop

### Phase 4: Equipment System
- Equip items to character slots
- Apply item stats to character
- Update character appearance (client-side)
- Protocol messages for equipment changes

### Phase 5: Vendor System
- NPC vendors with buy/sell lists
- Currency (copper/silver/gold/platinum)
- Buy items from vendor
- Sell items to vendor

---

## Testing

### Build Status
? **Build Successful** - No compilation errors

### Manual Testing
```cpp
// Test item loading
#include "req/shared/ItemLoader.h"

auto items = req::shared::data::loadItemTemplates("data/items/items.json");
assert(items.size() == 3);
assert(items[1001].name == "Cloth Shirt");
assert(items[1002].damage == 4);

// Test loot loading
std::uint32_t zoneId = 0;
auto loot = req::shared::data::loadLootTablesFromZoneFile(
    "data/loot/zone_10_loot.json", zoneId);
assert(zoneId == 10);
assert(loot[10].entries.size() == 3);
```

### Integration Testing
1. Load items.json successfully
2. Load zone_10_loot.json successfully
3. Cross-reference item_ids in loot tables with items
4. Verify enum conversions work correctly
5. Test JSON serialization round-trip

---

## Directory Structure

```
REQ_Backend/
??? REQ_Shared/
?   ??? include/
?   ?   ??? req/
?   ?       ??? shared/
?   ?           ??? DataModels.h        ? MODIFIED
?   ?           ??? ItemLoader.h        ? NEW
?   ??? src/
?       ??? ItemLoader.cpp              ? NEW
??? data/
    ??? items/
    ?   ??? items.json                  ? NEW
    ??? loot/
        ??? zone_10_loot.json           ? NEW
```

---

## Summary

**Status:** ? **Implementation Complete**

**What Was Added:**
1. ? Item and loot data structures in DataModels.h
2. ? ItemLoader module for loading items and loot tables
3. ? Seed JSON files for items and zone 10 loot
4. ? Full JSON serialization support
5. ? Enum helper functions
6. ? Comprehensive error handling

**What Works:**
- Items can be loaded from JSON
- Loot tables can be loaded from JSON
- Data structures serialize/deserialize correctly
- Error handling with logging
- Build successful

**What Doesn't Work Yet:**
- Items not integrated into gameplay
- Loot not generated from NPC deaths
- No inventory system
- No item pickup/drop
- No equipment system

**Build:** ? Successful
**Ready For:** Integration into ZoneServer and inventory systems

---

## Complete! ??

The item and loot system data infrastructure is ready. Next steps would be integrating this into ZoneServer for actual loot generation and creating the inventory/equipment systems.
