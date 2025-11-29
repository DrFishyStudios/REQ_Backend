# Zone Combat State Implementation - Complete ?

## Summary

The ZoneServer now tracks combat state for players (HP, mana, stats) and loads NPCs from JSON files. This provides the foundation for implementing AttackRequest/Result handlers.

---

## A) ? ZonePlayer Extended with Combat State

**File:** `REQ_ZoneServer/include/req/zone/ZoneServer.h`

### Added Fields

```cpp
struct ZonePlayer {
    // ...existing fields...
    
    // Combat state (loaded from character, persisted on zone exit)
    std::int32_t level{ 1 };
    std::int32_t hp{ 100 };
    std::int32_t maxHp{ 100 };
    std::int32_t mana{ 100 };
    std::int32_t maxMana{ 100 };
    
    // Primary stats (EQ-classic style)
    std::int32_t strength{ 75 };
    std::int32_t stamina{ 75 };
    std::int32_t agility{ 75 };
    std::int32_t dexterity{ 75 };
    std::int32_t intelligence{ 75 };
    std::int32_t wisdom{ 75 };
    std::int32_t charisma{ 75 };
    
    // Flags
    bool isDirty{ false };             // Position changed
    bool combatStatsDirty{ false };    // Combat stats changed
};
```

**Rationale:**
- Mirrors Character model but lives in zone memory for fast access
- Separate dirty flags allow granular persistence control
- All 7 EQ-classic stats included for future combat formulas

---

## B) ? Combat State Initialization

**File:** `REQ_ZoneServer/src/ZoneServer.cpp`
**Function:** `handleMessage()` - ZoneAuthRequest handler

### Implementation

```cpp
// After spawnPlayer() call:
player.level = character->level;
player.hp = (character->hp > 0) ? character->hp : character->maxHp;
player.maxHp = character->maxHp;
player.mana = (character->mana > 0) ? character->mana : character->maxMana;
player.maxMana = character->maxMana;

player.strength = character->strength;
player.stamina = character->stamina;
player.agility = character->agility;
player.dexterity = character->dexterity;
player.intelligence = character->intelligence;
player.wisdom = character->wisdom;
player.charisma = character->charisma;

player.combatStatsDirty = false;
```

### Logging

```
[INFO] [zone] [COMBAT] Initialized combat state: level=5, hp=80/100, mana=50/100
```

**HP Initialization Logic:**
- If `character->hp > 0`: Use saved HP (player might be injured)
- If `character->hp <= 0`: Reset to `maxHp` (fresh spawn or dead character)

---

## C) ? Combat State Persistence

**File:** `REQ_ZoneServer/src/ZoneServer.cpp`
**Function:** `savePlayerPosition()`

### Implementation

```cpp
// Update combat state if dirty
if (player.combatStatsDirty) {
    character->level = player.level;
    character->hp = player.hp;
    character->maxHp = player.maxHp;
    character->mana = player.mana;
    character->maxMana = player.maxMana;
    
    character->strength = player.strength;
    // ...all 7 stats...
    
    logInfo("zone", "[SAVE] Combat stats saved: characterId=..., hp=.../..., mana=.../...");
}

// Save to disk
characterStore_.saveCharacter(*character);

// Clear dirty flags
playerIt->second.isDirty = false;
playerIt->second.combatStatsDirty = false;
```

### When Saved

1. **Disconnect:** `removePlayer()` ? `savePlayerPosition()` ? writes HP/mana/stats
2. **Autosave:** Every 30s (default), if `combatStatsDirty == true`
3. **Manual:** Future admin commands

**combatStatsDirty Flag:**
- Set to `true` when HP/mana changes (future: damage, healing, buffs)
- Set to `false` after successful save
- Prevents unnecessary disk writes if stats unchanged

---

## D) ? ZoneNpc Struct

**File:** `REQ_Shared/include/req/shared/DataModels.h`

### Definition

```cpp
struct ZoneNpc {
    std::uint64_t npcId{ 0 };              // Unique NPC instance ID
    std::string name;                       // Display name
    std::int32_t level{ 1 };                // Level
    
    // Combat state
    std::int32_t currentHp{ 100 };
    std::int32_t maxHp{ 100 };
    bool isAlive{ true };
    
    // Position and orientation
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    float facingDegrees{ 0.0f };
    
    // Combat parameters
    std::int32_t minDamage{ 1 };
    std::int32_t maxDamage{ 5 };
    float aggroRadius{ 10.0f };             // Aggro range
    float leashRadius{ 50.0f };             // Distance before reset
    
    // Spawn point (for leashing)
    float spawnX{ 0.0f };
    float spawnY{ 0.0f };
    float spawnZ{ 0.0f };
};
```

**Design Notes:**
- `npcId`: Unique per zone (e.g., 1001, 1002 for zone 10)
- `currentHp`: Initialized to `maxHp`, decreases with damage
- `isAlive`: Set to `false` when `currentHp <= 0`
- Spawn point stored for leashing/respawn mechanics

---

## E) ? NPC JSON Schema

**File:** `config/zones/zone_<zoneId>_npcs.json`

### Example: zone_10_npcs.json

```json
{
  "npcs": [
    {
      "npc_id": 1001,
      "name": "A Decaying Skeleton",
      "level": 1,
      "max_hp": 20,
      "min_damage": 1,
      "max_damage": 3,
      "pos_x": 100.0,
      "pos_y": 50.0,
      "pos_z": 0.0,
      "facing_degrees": 0.0,
      "aggro_radius": 10.0,
      "leash_radius": 50.0
    }
  ]
}
```

### Field Definitions

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `npc_id` | uint64 | Yes | - | Unique ID (must be non-zero) |
| `name` | string | Yes | "Unknown NPC" | Display name |
| `level` | int32 | Yes | 1 | NPC level |
| `max_hp` | int32 | Yes | 100 | Maximum HP |
| `min_damage` | int32 | No | 1 | Minimum damage per hit |
| `max_damage` | int32 | No | 5 | Maximum damage per hit |
| `pos_x` | float | Yes | 0.0 | X coordinate |
| `pos_y` | float | Yes | 0.0 | Y coordinate |
| `pos_z` | float | Yes | 0.0 | Z coordinate (height) |
| `facing_degrees` | float | No | 0.0 | Facing direction (0-360) |
| `aggro_radius` | float | No | 10.0 | Aggro detection range |
| `leash_radius` | float | No | 50.0 | Distance before reset |

---

## F) ? NPC Loading

**File:** `REQ_ZoneServer/src/ZoneServer.cpp`
**Function:** `loadNpcsForZone()`

### Implementation Flow

1. **Construct path:** `config/zones/zone_<zoneId>_npcs.json`
2. **Open file:** If not found, log info and return (no NPCs)
3. **Parse JSON:** Extract `npcs` array
4. **For each NPC:**
   - Parse all fields
   - Validate `npc_id != 0`
   - Check for duplicates
   - Initialize `currentHp = maxHp`
   - Store spawn point for leashing
   - Add to `npcs_` map
5. **Log summary:** Total NPCs loaded

### Logging Example

```
[INFO] [zone] [NPC] Loading NPCs from: config/zones/zone_10_npcs.json
[INFO] [zone] [NPC] Loaded: id=1001, name="A Decaying Skeleton", level=1, maxHp=20, pos=(100,50,0), facing=0
[INFO] [zone] [NPC] Loaded: id=1002, name="A Rat", level=1, maxHp=10, pos=(200,75,0), facing=90
[INFO] [zone] [NPC] Loaded: id=1003, name="A Fire Beetle", level=2, maxHp=30, pos=(150,120,0), facing=180
[INFO] [zone] [NPC] Loaded: id=1004, name="A Snake", level=1, maxHp=15, pos=(50,200,0), facing=270
[INFO] [zone] [NPC] Loaded: id=1005, name="A Zombie", level=3, maxHp=50, pos=(300,150,0), facing=45
[INFO] [zone] [NPC] Loaded 5 NPC(s) for zone 10
```

### Error Handling

**File Not Found:**
```
[INFO] [zone] [NPC] No NPC file found (config/zones/zone_15_npcs.json), zone will have no NPCs
```

**Missing 'npcs' Array:**
```
[WARN] [zone] [NPC] NPC file does not contain 'npcs' array
```

**Invalid NPC ID:**
```
[WARN] [zone] [NPC] Skipping NPC with npc_id=0 (invalid)
```

**Duplicate ID:**
```
[WARN] [zone] [NPC] Duplicate npc_id=1001, skipping
```

---

## G) ? NPC Update Loop

**File:** `REQ_ZoneServer/src/ZoneServer.cpp`
**Function:** `updateSimulation()`

### Implementation

```cpp
// Update NPCs (placeholder for now - no AI yet)
for (auto& [npcId, npc] : npcs_) {
    updateNpc(npc, dt);
}

// Periodic NPC debug logging (every 5 seconds at 20Hz = 100 ticks)
static std::uint64_t npcLogCounter = 0;
if (!npcs_.empty() && ++npcLogCounter % 100 == 0) {
    logInfo("zone", "[NPC] Tick: " + std::to_string(npcs_.size()) + " NPC(s) in zone (no AI yet)");
}
```

### updateNpc() Placeholder

```cpp
void ZoneServer::updateNpc(ZoneNpc& npc, float deltaSeconds) {
    // Placeholder for future AI/behavior
    // For now, NPCs are stationary
    (void)npc;
    (void)deltaSeconds;
}
```

**Future Implementation:**
- Pathfinding/movement
- Aggro detection (check player distance vs `aggroRadius`)
- Leashing (if distance from spawn > `leashRadius`, return home)
- Auto-attack if in melee range
- Health regeneration if out of combat

---

## Sample NPC Data

### Zone 10 (East Freeport) - 5 NPCs

| NPC ID | Name | Level | HP | Damage | Position | Behavior |
|--------|------|-------|----|----|----------|----------|
| 1001 | A Decaying Skeleton | 1 | 20 | 1-3 | (100, 50, 0) | Weak undead |
| 1002 | A Rat | 1 | 10 | 1-2 | (200, 75, 0) | Critter |
| 1003 | A Fire Beetle | 2 | 30 | 2-5 | (150, 120, 0) | Low-level insect |
| 1004 | A Snake | 1 | 15 | 1-4 | (50, 200, 0) | Poisonous critter |
| 1005 | A Zombie | 3 | 50 | 3-7 | (300, 150, 0) | Stronger undead |

### Zone 20 (West Commonlands) - 3 NPCs

| NPC ID | Name | Level | HP | Damage | Position | Behavior |
|--------|------|-------|----|----|----------|----------|
| 2001 | A Gnoll Scout | 3 | 45 | 2-6 | (80, 120, 0) | Humanoid scout |
| 2002 | A Gnoll Warrior | 4 | 60 | 3-8 | (180, 90, 0) | Humanoid fighter |
| 2003 | An Orc Pawn | 5 | 80 | 4-10 | (250, 180, 0) | Tough humanoid |

---

## Integration with AttackRequest/Result (Next Step)

### How Combat Will Work

1. **Client sends AttackRequest:**
   ```
   Payload: attackerCharacterId|targetId|abilityId|isBasicAttack
   Example: "42|1001|0|1"  (player 42 attacks NPC 1001, basic attack)
   ```

2. **ZoneServer processes attack:**
   ```cpp
   case MessageType::AttackRequest: {
       // Parse request
       AttackRequestData request;
       parseAttackRequestPayload(body, request);
       
       // Validate attacker is sender's character
       // Look up target NPC in npcs_ map
       auto npcIt = npcs_.find(request.targetId);
       
       // Check range (distance between player and NPC)
       float dx = player.posX - npc.posX;
       float dy = player.posY - npc.posY;
       float distance = sqrt(dx*dx + dy*dy);
       
       if (distance > 5.0f) {
           // Out of range
           sendAttackResult(request.attackerId, request.targetId, 0, false, npc.currentHp, 1, "Target out of range");
           return;
       }
       
       // Calculate damage (simple formula for now)
       int damage = player.level + (player.strength / 10) + rand(npc.minDamage, npc.maxDamage);
       
       // Apply damage
       npc.currentHp -= damage;
       if (npc.currentHp <= 0) {
           npc.currentHp = 0;
           npc.isAlive = false;
       }
       
       // Send result to attacker (and nearby players)
       sendAttackResult(request.attackerId, request.targetId, damage, true, npc.currentHp, 0, "Hit for " + std::to_string(damage) + " damage");
       
       break;
   }
   ```

3. **Client receives AttackResult:**
   ```
   Payload: attackerId|targetId|damage|wasHit|remainingHp|resultCode|message
   Example: "42|1001|15|1|5|0|Hit for 15 damage"
   ```

### Data Structures Ready

? **ZonePlayer has:**
- `level`, `strength`, `hp` for damage calculation
- `posX`, `posY`, `posZ` for range checks

? **ZoneNpc has:**
- `npcId`, `name`, `level` for targeting
- `currentHp`, `maxHp`, `isAlive` for damage application
- `posX`, `posY`, `posZ` for range checks
- `minDamage`, `maxDamage` for NPC counter-attacks (future)
- `aggroRadius` for aggro detection

? **AttackRequest/Result messages:**
- Already defined in ProtocolSchemas
- Build/parse helpers implemented
- MessageType enum updated

**Next Implementation Steps:**
1. Add `AttackRequest` handler to `ZoneServer::handleMessage()`
2. Implement range validation
3. Implement damage calculation (level + stats + random)
4. Apply damage to NPCs (decrement `currentHp`)
5. Send `AttackResult` to attacker
6. Optional: Broadcast `AttackResult` to nearby players
7. Optional: NPC counter-attack if aggro'd

---

## Testing Checklist

### Unit Tests (Build)
- [x] ZonePlayer struct compiles with combat fields
- [x] ZoneNpc struct compiles
- [x] NPC JSON loading compiles
- [x] Combat state init/save compiles

### Integration Tests (Runtime)
- [ ] Start ZoneServer with zone_10_npcs.json
- [ ] Verify 5 NPCs loaded with correct stats
- [ ] Player enters zone
- [ ] Verify player combat state initialized from character
- [ ] Player disconnects
- [ ] Verify player HP/mana saved to character file
- [ ] Restart server
- [ ] Verify player HP/mana restored on re-entry

### Manual Test Procedure

**Step 1: Start Servers**
```bash
cd x64/Debug
.\REQ_LoginServer.exe
.\REQ_WorldServer.exe
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="East Freeport"
```

**Step 2: Check ZoneServer Logs for NPC Loading**
```
[INFO] [zone] [NPC] Loading NPCs from: config/zones/zone_10_npcs.json
[INFO] [zone] [NPC] Loaded: id=1001, name="A Decaying Skeleton", ...
[INFO] [zone] [NPC] Loaded 5 NPC(s) for zone 10
```

**Step 3: Connect TestClient**
```bash
.\REQ_TestClient.exe
# Login as testuser
# Enter world
```

**Step 4: Check ZoneServer Logs for Combat State Init**
```
[INFO] [zone] [COMBAT] Initialized combat state: level=1, hp=100/100, mana=100/100
```

**Step 5: Disconnect TestClient**

**Step 6: Check ZoneServer Logs for Combat State Save**
```
[INFO] [zone] [SAVE] Position saved: characterId=1, ...
```

**Step 7: Check Character JSON File**
```bash
cat data/characters/1.json
# Verify hp, maxHp, mana, maxMana, strength, etc. fields are present
```

---

## Files Modified/Created

### Modified
1. `REQ_ZoneServer/include/req/zone/ZoneServer.h`
   - Extended `ZonePlayer` with combat fields
   - Added `npcs_` container
   - Added `loadNpcsForZone()` and `updateNpc()` declarations

2. `REQ_ZoneServer/src/ZoneServer.cpp`
   - Implemented combat state initialization in ZoneAuth handler
   - Implemented combat state persistence in `savePlayerPosition()`
   - Implemented `loadNpcsForZone()` with JSON parsing
   - Added NPC update loop to `updateSimulation()`
   - Added required includes (`<fstream>`, `nlohmann/json.hpp`)

3. `REQ_Shared/include/req/shared/DataModels.h`
   - Added `ZoneNpc` struct

### Created
4. `config/zones/zone_10_npcs.json` - 5 sample NPCs for zone 10
5. `config/zones/zone_20_npcs.json` - 3 sample NPCs for zone 20
6. `config/ZONE_COMBAT_STATE.md` - This documentation

---

## Build Status

? **Build Successful**
- No compilation errors
- No warnings
- All projects recompiled successfully

---

## Summary

? **A) ZonePlayer extended** - Added level, hp, maxHp, mana, maxMana, and all 7 primary stats  
? **B) Combat state initialized** - On ZoneAuth from character data  
? **C) Combat state persisted** - On disconnect and autosave with `combatStatsDirty` flag  
? **D) ZoneNpc struct defined** - Includes HP, level, position, damage, aggro/leash  
? **E) NPC JSON schema** - Simple, extensible format  
? **F) NPC loading** - Parses JSON, validates, logs each NPC  
? **G) NPC update loop** - Placeholder for future AI  

**The zone-side combat foundation is complete! Ready to implement AttackRequest/Result handlers.** ??

---

## Quick Reference

### Check NPC Loading
```bash
grep "\[NPC\]" zone_server.log
```

### Check Combat State
```bash
grep "\[COMBAT\]" zone_server.log
```

### Check Combat Saves
```bash
grep "\[SAVE\] Combat stats saved" zone_server.log
```

### Add New NPC
Edit `config/zones/zone_<zoneId>_npcs.json`:
```json
{
  "npc_id": 1006,
  "name": "A New Monster",
  "level": 5,
  "max_hp": 100,
  "min_damage": 5,
  "max_damage": 10,
  "pos_x": 400.0,
  "pos_y": 200.0,
  "pos_z": 0.0,
  "facing_degrees": 0.0,
  "aggro_radius": 15.0,
  "leash_radius": 75.0
}
```

Restart ZoneServer to load new NPC.
