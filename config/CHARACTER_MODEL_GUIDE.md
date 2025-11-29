# Character Model - MMO Enhancement Guide

## Overview

The Character model has been extended with MMO-style progression, stats, and location tracking, transforming it from a simple placeholder into a full-featured RPG character system inspired by EverQuest classic.

---

## Character Model Structure

### Complete Field List

```cpp
struct Character {
    // Identity
    uint64_t characterId;
    uint64_t accountId;
    string name;
    string race;
    string characterClass;
    
    // Progression
    uint32_t level;           // Current level (1-60 in EQ classic)
    uint64_t xp;              // Experience points
    
    // Vitals
    int32_t hp;               // Current hit points
    int32_t maxHp;            // Maximum hit points
    int32_t mana;             // Current mana
    int32_t maxMana;          // Maximum mana
    
    // Primary Stats (EQ-classic style)
    int32_t strength;         // Physical power, melee damage
    int32_t stamina;          // Hit points, fatigue resistance
    int32_t agility;          // Armor class, attack speed
    int32_t dexterity;        // Precision, ranged attacks
    int32_t intelligence;     // Mana pool (for INT casters)
    int32_t wisdom;           // Mana pool (for WIS casters)
    int32_t charisma;         // Merchant prices, charm effectiveness
    
    // World and Zone Tracking
    uint32_t homeWorldId;     // Original world (character creation)
    uint32_t lastWorldId;     // Last world logged into
    uint32_t lastZoneId;      // Last zone entered
    
    // Last Known Position
    float positionX;
    float positionY;
    float positionZ;
    float heading;            // Facing direction (0-360 degrees)
    
    // Bind Point (Respawn Location)
    int32_t bindWorldId;      // -1 = not set
    int32_t bindZoneId;       // -1 = not set
    float bindX;
    float bindY;
    float bindZ;
    
    // Inventory (Placeholder)
    vector<string> inventorySlots;
};
```

---

## JSON Schema

### Example Character JSON

```json
{
  "character_id": 1,
  "account_id": 1,
  "name": "Arthas",
  "race": "Human",
  "class": "Paladin",
  
  "level": 1,
  "xp": 0,
  
  "hp": 135,
  "max_hp": 135,
  "mana": 125,
  "max_mana": 125,
  
  "strength": 80,
  "stamina": 80,
  "agility": 75,
  "dexterity": 75,
  "intelligence": 75,
  "wisdom": 85,
  "charisma": 80,
  
  "home_world_id": 1,
  "last_world_id": 1,
  "last_zone_id": 10,
  
  "position_x": 0.0,
  "position_y": 0.0,
  "position_z": 0.0,
  "heading": 0.0,
  
  "bind_world_id": 1,
  "bind_zone_id": 10,
  "bind_x": 0.0,
  "bind_y": 0.0,
  "bind_z": 0.0,
  
  "inventory_slots": []
}
```

### Field Descriptions

| JSON Field | Type | Default | Description |
|------------|------|---------|-------------|
| `character_id` | uint64 | 0 | Unique character identifier |
| `account_id` | uint64 | 0 | Account that owns this character |
| `name` | string | "" | Character name (must be unique) |
| `race` | string | "" | EQ-classic race (Human, Elf, Dwarf, etc.) |
| `class` | string | "" | EQ-classic class (Warrior, Cleric, etc.) |
| `level` | uint32 | 1 | Character level (1-60) |
| `xp` | uint64 | 0 | Experience points |
| `hp` | int32 | 100 | Current hit points |
| `max_hp` | int32 | 100 | Maximum hit points |
| `mana` | int32 | 100 | Current mana |
| `max_mana` | int32 | 100 | Maximum mana (0 for non-casters) |
| `strength` | int32 | 75 | STR stat (melee damage, carrying capacity) |
| `stamina` | int32 | 75 | STA stat (HP, fatigue) |
| `agility` | int32 | 75 | AGI stat (AC, attack speed) |
| `dexterity` | int32 | 75 | DEX stat (accuracy, ranged attacks) |
| `intelligence` | int32 | 75 | INT stat (mana for INT casters) |
| `wisdom` | int32 | 75 | WIS stat (mana for WIS casters) |
| `charisma` | int32 | 75 | CHA stat (charm, merchant prices) |
| `home_world_id` | uint32 | 0 | Original world |
| `last_world_id` | uint32 | 0 | Last world visited |
| `last_zone_id` | uint32 | 0 | Last zone entered |
| `position_x` | float | 0.0 | X coordinate |
| `position_y` | float | 0.0 | Y coordinate |
| `position_z` | float | 0.0 | Z coordinate |
| `heading` | float | 0.0 | Facing direction (degrees) |
| `bind_world_id` | int32 | -1 | Bind point world (-1 = not set) |
| `bind_zone_id` | int32 | -1 | Bind point zone (-1 = not set) |
| `bind_x` | float | 0.0 | Bind point X |
| `bind_y` | float | 0.0 | Bind point Y |
| `bind_z` | float | 0.0 | Bind point Z |
| `inventory_slots` | array | [] | Inventory items (placeholder) |

---

## Stat Calculation

### Base Stats

All characters start with baseline stats of **75** in each attribute, then apply racial and class modifiers.

### Racial Modifiers

Based on EverQuest classic racial stat modifiers:

| Race | STR | STA | AGI | DEX | INT | WIS | CHA |
|------|-----|-----|-----|-----|-----|-----|-----|
| Human | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Barbarian | +10 | +10 | -5 | 0 | -5 | -5 | -5 |
| Erudite | -10 | -5 | -5 | -5 | +10 | +10 | -5 |
| Wood Elf | -5 | -5 | +10 | 0 | 0 | +5 | 0 |
| High Elf | -10 | -5 | +10 | 0 | +10 | +5 | +5 |
| Dark Elf | -10 | -5 | +15 | +10 | +10 | 0 | 0 |
| Half Elf | 0 | -5 | +10 | 0 | 0 | 0 | +5 |
| Dwarf | +10 | +10 | -5 | +5 | -5 | +5 | -10 |
| Troll | +15 | +15 | -5 | -5 | -10 | -5 | -15 |
| Ogre | +20 | +15 | -10 | -10 | -10 | -5 | -15 |
| Halfling | -10 | -5 | +15 | +15 | -5 | 0 | +5 |
| Gnome | -10 | -5 | +10 | +10 | +10 | 0 | +5 |

### Class Modifiers

Small bonuses to primary stats:

| Class | STR | STA | AGI | DEX | INT | WIS | CHA |
|-------|-----|-----|-----|-----|-----|-----|-----|
| Warrior | +5 | +5 | 0 | 0 | 0 | 0 | 0 |
| Cleric | 0 | 0 | 0 | 0 | 0 | +5 | +5 |
| Paladin | +5 | +5 | 0 | 0 | 0 | +5 | +5 |
| Ranger | +5 | +5 | +5 | 0 | 0 | +5 | 0 |
| Shadow Knight | +5 | +5 | 0 | 0 | +5 | 0 | 0 |
| Druid | 0 | 0 | 0 | 0 | 0 | +10 | 0 |
| Monk | +5 | +5 | +10 | +10 | 0 | 0 | 0 |
| Bard | 0 | 0 | +5 | +10 | 0 | 0 | +10 |
| Rogue | 0 | 0 | +10 | +15 | 0 | 0 | 0 |
| Shaman | 0 | 0 | 0 | 0 | 0 | +10 | +5 |
| Necromancer | 0 | 0 | 0 | +10 | +10 | 0 | 0 |
| Wizard | 0 | 0 | 0 | 0 | +15 | 0 | 0 |
| Magician | 0 | 0 | 0 | 0 | +15 | 0 | 0 |
| Enchanter | 0 | 0 | 0 | 0 | +10 | 0 | +10 |

### Example: Troll Warrior

```
Base stats: 75 in all
Troll racial: STR+15, STA+15, AGI-5, DEX-5, INT-10, WIS-5, CHA-15
Warrior class: STR+5, STA+5

Final stats:
  STR: 75 + 15 + 5 = 95
  STA: 75 + 15 + 5 = 95
  AGI: 75 - 5 = 70
  DEX: 75 - 5 = 70
  INT: 75 - 10 = 65
  WIS: 75 - 5 = 70
  CHA: 75 - 15 = 60
```

### HP Calculation

Base HP formula (simplified):

```
baseHP = 100
+ class modifier (Warrior: +20, Melee: +15, Hybrid: +10, Priest: +5, Caster: 0)
+ race modifier (Barbarian/Troll/Ogre: +10, Dwarf: +5, Gnome/Halfling: -5)
```

**Examples:**
- Human Warrior: 100 + 20 + 0 = **120 HP**
- Troll Warrior: 100 + 20 + 10 = **130 HP**
- Gnome Wizard: 100 + 0 - 5 = **95 HP**

### Mana Calculation

**Non-casters (Warrior, Rogue, Monk): 0 mana**

Base mana formula:

```
baseMana = 100
+ class modifier (Pure Caster: +20, Priest: +15, Hybrid: +10)
+ race modifier (Erudite/HighElf: +10, DarkElf/Gnome: +5)
```

**Examples:**
- Erudite Wizard: 100 + 20 + 10 = **130 mana**
- Human Cleric: 100 + 15 + 0 = **115 mana**
- Human Paladin: 100 + 10 + 0 = **110 mana**

---

## Backward Compatibility

### Loading Old Character Files

Old character JSON files missing new fields will automatically get defaults:

| Missing Field | Default Value |
|---------------|---------------|
| `xp` | 0 |
| `hp` | 100 |
| `max_hp` | 100 |
| `mana` | 100 |
| `max_mana` | 100 |
| `strength` | 75 |
| `stamina` | 75 |
| `agility` | 75 |
| `dexterity` | 75 |
| `intelligence` | 75 |
| `wisdom` | 75 |
| `charisma` | 75 |
| `bind_world_id` | -1 |
| `bind_zone_id` | -1 |
| `bind_x` | 0.0 |
| `bind_y` | 0.0 |
| `bind_z` | 0.0 |

### Upgrading Existing Characters

Use the `REQ_CharacterUpgrade` utility to upgrade all existing character files:

```powershell
cd x64\Debug
.\REQ_CharacterUpgrade.exe
```

This will:
1. Load each character JSON file
2. Apply defaults for missing fields
3. Re-save with full schema

**Example Output:**
```
=== REQ Character JSON Upgrade Utility ===
Characters directory: data/characters

Processing character ID 1... OK (upgraded: Arthas, Human Paladin level 1)
Processing character ID 2... OK (upgraded: Thrall, Orc Shaman level 1)

=== Summary ===
Upgraded: 2
Skipped:  0
Errors:   0

All character JSON files have been upgraded to the new schema.
```

---

## Character Creation Helper

### Using createDefaultCharacter

The `CharacterStore::createDefaultCharacter()` static helper initializes all MMO fields:

```cpp
auto character = CharacterStore::createDefaultCharacter(
    accountId,       // Account that owns this character
    homeWorldId,     // Home world
    homeZoneId,      // Starting zone
    "Arthas",        // Character name
    "Human",         // Race
    "Paladin",       // Class
    100.0f,          // Starting X
    200.0f,          // Starting Y
    10.0f            // Starting Z
);

// character now has:
// - Level 1, XP 0
// - HP/Mana calculated for Human Paladin
// - Stats calculated for Human Paladin
// - Position set to (100, 200, 10)
// - Bind point set to same location
```

### Integration with WorldServer

The `CharacterStore::createCharacterForAccount()` method now uses the helper internally:

```cpp
// Old way (manual field initialization):
character.level = 1;
character.positionX = 0.0f;
// ... many more fields ...

// New way (automatic):
auto character = characterStore.createCharacterForAccount(
    accountId,
    homeWorldId,
    "Arthas",
    "Human",
    "Paladin"
);
// All fields initialized automatically with proper race/class bonuses
```

---

## Future Enhancements

### Planned Features

1. **Level-up System:**
   - XP required per level tables
   - Stat gains on level-up
   - HP/Mana scaling formulas

2. **Starting Locations Table:**
   - Race-specific starting cities
   - Class-specific guild positions
   - Proper spawn coordinates

3. **Race/Class Restrictions:**
   - Enforce valid combinations (e.g., Ogre can't be Paladin)
   - Load from data tables

4. **Derived Stats:**
   - Armor Class (AGI-based)
   - Attack Speed (AGI/DEX-based)
   - Mana Regen (WIS/INT-based)
   - HP Regen (STA-based)

5. **Equipment Stats:**
   - Item bonuses to stats
   - Equipped item tracking
   - Stat recalculation on equip/unequip

6. **Alternate Advancement:**
   - AA points
   - AA abilities
   - AA stat bonuses

---

## Testing

### Verify Character Creation

```powershell
# Start WorldServer
.\REQ_WorldServer.exe

# Use TestClient to create a character
.\REQ_TestClient.exe --happy-path
```

**Check character JSON:**
```json
{
  "character_id": 1,
  "name": "TestWarrior",
  "race": "Human",
  "class": "Warrior",
  
  "level": 1,
  "xp": 0,
  
  "hp": 120,
  "max_hp": 120,
  "mana": 0,
  "max_mana": 0,
  
  "strength": 80,
  "stamina": 80,
  "agility": 75,
  ...
}
```

### Verify Stat Calculations

Create characters of different races/classes and verify stats match tables:

```cpp
// Barbarian Warrior should have high STR/STA
auto barb = CharacterStore::createDefaultCharacter(
    1, 1, 10, "Conan", "Barbarian", "Warrior", 0, 0, 0);
// STR: 75 + 10 (race) + 5 (class) = 90
// STA: 75 + 10 (race) + 5 (class) = 90
```

---

## Summary

? **Character Model Extended** - Full MMO stats and progression  
? **Backward Compatible** - Old JSON files load with defaults  
? **Automatic Stat Calculation** - Race/class bonuses applied  
? **HP/Mana Formulas** - Class and race-appropriate values  
? **Bind Point System** - Respawn location tracking  
? **Helper Functions** - Easy character creation with proper initialization  
? **Upgrade Utility** - Tool to migrate existing characters  
? **Build Success** - All servers compile without errors

The character persistence system now supports a complete MMO-style character with stats, progression, and location tracking!
