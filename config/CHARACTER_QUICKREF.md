# Character Model - Quick Reference

## TL;DR

Characters now have full MMO stats, XP, HP/Mana, racial/class bonuses, and bind points.

---

## Character Fields

### Core Identity
- `characterId` - Unique ID
- `accountId` - Owner
- `name` - Character name
- `race` - EQ race (Human, Elf, Dwarf, etc.)
- `characterClass` - EQ class (Warrior, Cleric, etc.)

### Progression
- `level` - Current level (1-60)
- `xp` - Experience points

### Vitals
- `hp` / `maxHp` - Hit points
- `mana` / `maxMana` - Mana (0 for warriors/rogues/monks)

### Stats (Base 75 + racial + class modifiers)
- `strength` - Melee damage, carrying capacity
- `stamina` - HP, fatigue
- `agility` - AC, attack speed
- `dexterity` - Accuracy, ranged
- `intelligence` - Mana (INT casters)
- `wisdom` - Mana (WIS casters)
- `charisma` - Charm, merchant prices

### Location
- `homeWorldId` - Original world
- `lastWorldId` - Last world visited
- `lastZoneId` - Last zone entered
- `positionX/Y/Z` - Current position
- `heading` - Facing direction (0-360°)

### Bind Point (Respawn)
- `bindWorldId` - Bind world (-1 = not set)
- `bindZoneId` - Bind zone (-1 = not set)
- `bindX/Y/Z` - Bind position

---

## Quick Stats Table

### Racial Bonuses

| Race | Notable Bonuses |
|------|----------------|
| Human | Balanced (no bonuses) |
| Barbarian | STR +10, STA +10 |
| Erudite | INT +10, WIS +10, STR -10 |
| Troll | STR +15, STA +15, very low CHA |
| Ogre | STR +20, STA +15, very low INT/CHA |
| High Elf | INT +10, AGI +10, CHA +5 |
| Dark Elf | AGI +15, DEX +10, INT +10 |
| Dwarf | STR +10, STA +10, WIS +5 |
| Gnome | INT +10, AGI +10, DEX +10 |

### Class Bonuses

| Class | Primary Stat Bonuses |
|-------|---------------------|
| Warrior | STR +5, STA +5 |
| Cleric | WIS +5, CHA +5 |
| Paladin | STR +5, STA +5, WIS +5, CHA +5 |
| Wizard | INT +15 |
| Necromancer | INT +10, DEX +10 |
| Rogue | AGI +10, DEX +15 |
| Monk | STR +5, STA +5, AGI +10, DEX +10 |

---

## HP and Mana

### HP Formula
```
Base: 100
+ Class: Warrior +20, Melee +15, Hybrid +10, Priest +5, Caster +0
+ Race: Barb/Troll/Ogre +10, Dwarf +5, Gnome/Halfling -5
```

**Examples:**
- Human Warrior: 120 HP
- Troll Warrior: 130 HP
- Gnome Wizard: 95 HP

### Mana Formula

**Warriors/Rogues/Monks: 0 mana**

```
Base: 100
+ Class: Pure Caster +20, Priest +15, Hybrid +10
+ Race: Erudite/HighElf +10, DarkElf/Gnome +5
```

**Examples:**
- Erudite Wizard: 130 mana
- Human Cleric: 115 mana
- Human Paladin: 110 mana

---

## Character Creation

### Using Helper Function

```cpp
#include "../../REQ_Shared/include/req/shared/CharacterStore.h"

auto character = CharacterStore::createDefaultCharacter(
    accountId,       // 1
    homeWorldId,     // 1
    homeZoneId,      // 10 (East Freeport)
    "Arthas",        // Name
    "Human",         // Race
    "Paladin",       // Class
    100.0f,          // Start X
    200.0f,          // Start Y
    10.0f            // Start Z
);

// character.characterId = 0;  // Caller must assign ID
// character.level = 1;
// character.xp = 0;
// character.hp = 135;  // Calculated for Human Paladin
// character.maxHp = 135;
// character.mana = 125;
// character.maxMana = 125;
// character.strength = 80;  // 75 + 5 (class)
// character.wisdom = 85;    // 75 + 5 (class) + 5 (class again)
// ... all other fields initialized
```

---

## JSON Example

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

---

## Backward Compatibility

### Loading Old Files

Old character JSON files automatically get defaults:

| Missing Field | Default |
|---------------|---------|
| `xp` | 0 |
| `hp`, `max_hp` | 100 |
| `mana`, `max_mana` | 100 |
| All stats | 75 |
| Bind point | -1 (not set) |

### Upgrading Files

```powershell
.\REQ_CharacterUpgrade.exe
```

Loads all characters and re-saves with full schema.

---

## Common Examples

### Barbarian Warrior (Tank)
```
STR: 90 (75 + 10 race + 5 class)
STA: 90 (75 + 10 race + 5 class)
AGI: 70 (75 - 5 race)
HP: 130 (100 + 20 class + 10 race)
Mana: 0
```

### Erudite Wizard (Pure Caster)
```
STR: 65 (75 - 10 race)
INT: 100 (75 + 10 race + 15 class)
WIS: 85 (75 + 10 race)
HP: 95 (100 + 0 class - 5 race)
Mana: 130 (100 + 20 class + 10 race)
```

### Dark Elf Necromancer (DPS Caster)
```
AGI: 90 (75 + 15 race)
DEX: 95 (75 + 10 race + 10 class)
INT: 95 (75 + 10 race + 10 class)
HP: 100 (base, no modifiers)
Mana: 125 (100 + 20 class + 5 race)
```

---

## API Reference

### CharacterStore Methods

```cpp
// Load by ID
std::optional<Character> loadById(uint64_t characterId);

// Load all for account/world
std::vector<Character> loadCharactersForAccountAndWorld(
    uint64_t accountId, uint32_t worldId);

// Create character (automatic stat initialization)
Character createCharacterForAccount(
    uint64_t accountId,
    uint32_t homeWorldId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass);

// Save character
bool saveCharacter(const Character& character);

// Helper: Create with custom position
static Character createDefaultCharacter(
    uint64_t accountId,
    uint32_t homeWorldId,
    uint32_t homeZoneId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass,
    float startX = 0.0f,
    float startY = 0.0f,
    float startZ = 0.0f);
```

---

## Testing

### Create Character via TestClient

```powershell
.\REQ_TestClient.exe --happy-path
```

Creates "TestWarrior" (Human Warrior) with:
- 120 HP
- 0 Mana
- STR/STA 80, other stats 75

### Verify JSON

Check `data/characters/<id>.json`:
- All stat fields present
- HP/Mana match expected values
- Bind point set to starting location

---

## Future Enhancements

- [ ] Level-up XP tables
- [ ] Stat gains on level-up
- [ ] Starting location tables (by race)
- [ ] Race/class restriction enforcement
- [ ] Derived stats (AC, regen, etc.)
- [ ] Equipment stat bonuses

---

## Support

**Full Documentation:**
- `config/CHARACTER_MODEL_GUIDE.md` - Complete reference

**Character Upgrade:**
- Use `REQ_CharacterUpgrade.exe` to migrate old files

**Build Status:**
- ? All servers compile successfully
- ? Backward compatible with old JSON files
