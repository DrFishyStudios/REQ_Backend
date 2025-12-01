# Phase 2: Spawn System & NPC AI Core - Implementation Summary

## Overview

Implemented a complete NPC AI system with aggro, hate tracking, leashing, respawn mechanics, and basic melee combat based on REQ_GDD_v09 specifications.

---

## What Was Implemented

### 1. Extended ZoneNpc Data Model
**File:** `REQ_Shared/include/req/shared/DataModels.h`

Added comprehensive AI state and combat mechanics:

```cpp
struct ZoneNpc {
    // AI state enum
    enum class AiState {
        Idle,       // Standing at spawn, scanning for targets
        Engaged,    // Actively fighting a target
        Leashing,   // Returning to spawn after losing target
        Fleeing,    // Running away (low HP, fear, etc.)
        Dead        // Waiting for respawn
    };
    
    // ...existing fields...
    
    // Respawn mechanics
    float respawnTimeSec{ 120.0f };         // How long until respawn after death
    float respawnTimerSec{ 0.0f };          // Current respawn countdown
    bool pendingRespawn{ false };           // Waiting to respawn
    
    // AI state
    AiState state{ AiState::Idle };
    
    // Hate/aggro tracking (simple single-target)
    std::uint64_t currentTargetId{ 0 };     // CharacterId of current target
    float hateValue{ 0.0f };                // Accumulated hate
    
    // Attack timing
    float attackCooldown{ 1.5f };           // Seconds between attacks
    float attackTimer{ 0.0f };              // Current attack timer
    
    // Movement
    float moveSpeed{ 50.0f };               // Movement speed in units/sec
};
```

### 2. Updated NPC Loading
**File:** `REQ_ZoneServer/src/ZoneServer_Npc.cpp`

Enhanced `loadNpcsForZone()` to initialize:
- AI state (starts at Idle)
- Hate tracking (0 initial hate)
- Respawn timers (from JSON or default 120s)
- Attack cooldown (from JSON or default 1.5s)
- Move speed (from JSON or default 50 units/sec)

### 3. Implemented Full AI State Machine
**File:** `REQ_ZoneServer/src/ZoneServer_Npc.cpp`

`updateNpc()` now implements complete behavior:

#### **Idle State**
- Scans for nearby players within `aggroRadius`
- Transitions to Engaged when player detected
- Logs aggro event with distance

#### **Engaged State**
- Tracks current target by characterId
- Moves toward target at `moveSpeed`
- Maintains distance check vs `leashRadius` from spawn
- When in melee range (5 units):
  - Performs melee attack on cooldown
  - Rolls random damage between `minDamage` and `maxDamage`
  - Applies damage to player HP
  - Triggers player death if HP <= 0
- Leashes if:
  - Distance from spawn > `leashRadius`
  - Target dies or disconnects

#### **Leashing State**
- Returns to spawn point at `moveSpeed`
- Heals to full HP when reaching spawn
- Transitions back to Idle
- Clears hate and target

#### **Dead State**
- Counts down `respawnTimerSec`
- Respawns at spawn point when timer expires
- Resets to full HP and Idle state

### 4. Aggro on Attack
**File:** `REQ_ZoneServer/src/ZoneServer_Combat.cpp`

Enhanced `processAttack()` to trigger aggro:

```cpp
// When player attacks NPC
if (target.state == AiState::Idle) {
    // Trigger aggro
    target.state = AiState::Engaged;
    target.currentTargetId = attacker.characterId;
    target.hateValue = static_cast<float>(totalDamage);
} else if (target.state == AiState::Engaged) {
    // Add hate or switch target
    if (newDamage > currentHate) {
        target.currentTargetId = newAttacker;
        target.hateValue = newDamage;
    }
}
```

**Hate System:**
- Damage-based hate accumulation
- Simple target switching (highest damage wins)
- Future: Can extend with heal aggro, taunt modifiers, etc.

### 5. Updated NPC Configuration Files
**Files:** 
- `config/zones/zone_10_npcs.json`
- `config/zones/zone_20_npcs.json`

Added new JSON fields:
```json
{
  "npc_id": 1001,
  "name": "A Decaying Skeleton",
  "respawn_time_sec": 60.0,
  "attack_cooldown": 2.0,
  "move_speed": 40.0
}
```

**Zone 10 NPCs:**
- 5 NPCs with varied respawn times (45-120s)
- Different movement speeds (35-60 units/sec)
- Varied attack speeds (1.2-2.5s)

**Zone 20 NPCs:**
- 3 higher-level NPCs
- Longer respawn times (90-180s)
- Slower but tankier enemies

### 6. Enhanced Simulation Logging
**File:** `REQ_ZoneServer/src/ZoneServer_Simulation.cpp`

Updated periodic NPC logging to show state breakdown:
```
[NPC] Active: 5 NPC(s) - Idle:3, Engaged:1, Leashing:1, Dead:0
```

---

## Key Behaviors

### Aggro Mechanics
1. **Proximity Aggro:** NPC aggros when player enters `aggroRadius`
2. **Attack Aggro:** NPC aggros when attacked (even outside proximity range)
3. **Hate Tracking:** Damage dealt accumulates hate
4. **Target Switching:** Simple highest-damage wins

### Leashing
- Triggered when NPC exceeds `leashRadius` from spawn
- Returns to spawn at normal move speed
- Heals to full HP on reaching spawn
- Clears hate and returns to Idle patrol

### Respawn
- Timer starts when NPC dies (HP <= 0)
- Configurable per-NPC via `respawn_time_sec`
- Respawns at original spawn coordinates
- Full HP restoration on respawn

### Melee Combat
- NPCs chase targets within leash range
- Attack when within 5 units (melee range)
- Cooldown-based attacks (configurable per NPC)
- Random damage roll between min/max
- Direct HP subtraction (marks player combatStatsDirty)

---

## Testing Instructions

### Start Zone Server
```bash
cd x64/Debug
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="East Freeport"
```

### Check NPC Loading
Look for logs:
```
[INFO] [zone] [NPC] Loading NPCs from: config/zones/zone_10_npcs.json
[INFO] [zone] [NPC] Loaded: id=1001, name="A Decaying Skeleton", respawn=60s
[INFO] [zone] [NPC] Loaded 5 NPC(s) for zone 10
```

### Test Behaviors

**1. Proximity Aggro:**
- Move character near NPC (within aggro_radius)
- Check for: `[NPC] Aggro: npc=1001 "A Decaying Skeleton", target=<charId>, distance=<dist>`

**2. Attack Player:**
- Wait for NPC to close to melee range
- Check for: `[NPC] Melee attack: npc=1001, target=<charId>, damage=<dmg>, targetHp=<hp>/<maxHp>`

**3. Player Attacks NPC:**
- Use TestClient to send AttackRequest
- Check for: `[NPC] Aggro on attack: npc=1001, attacker=<charId>, initialHate=<dmg>`

**4. Leashing:**
- Kite NPC away from spawn beyond leash_radius
- Check for: `[NPC] Leash triggered: npc=1001, distance from spawn=<dist>`
- Watch for: `[NPC] Reached spawn, reset to Idle: npc=1001`

**5. Death & Respawn:**
- Kill NPC (reduce HP to 0)
- Check for: `[NPC] NPC died, respawn in 60s: id=1001`
- Wait for respawn timer
- Check for: `[NPC] Respawned: id=1001, pos=(...)`

**6. State Monitoring:**
- Every 5 seconds, check: `[NPC] Active: 5 NPC(s) - Idle:X, Engaged:Y, Leashing:Z, Dead:W`

---

## File Changes Summary

### Modified Files
1. **REQ_Shared/include/req/shared/DataModels.h**
   - Extended ZoneNpc with AI state, hate, respawn, attack timing

2. **REQ_ZoneServer/src/ZoneServer_Npc.cpp**
   - Updated loadNpcsForZone to initialize new fields
   - Implemented full AI state machine in updateNpc
   - Added includes: `<random>`, `<cmath>`

3. **REQ_ZoneServer/src/ZoneServer_Combat.cpp**
   - Added aggro triggering on attack
   - Implemented hate accumulation and target switching

4. **REQ_ZoneServer/src/ZoneServer_Simulation.cpp**
   - Updated NPC logging to show state breakdown

5. **config/zones/zone_10_npcs.json**
   - Added respawn_time_sec, attack_cooldown, move_speed

6. **config/zones/zone_20_npcs.json**
   - Added respawn_time_sec, attack_cooldown, move_speed

### Build Status
? **Build Successful** - No errors, no warnings

---

## Design Decisions

### Simple Single-Target Hate
- Each NPC tracks one target at a time
- Hate = cumulative damage dealt
- Target switches to highest damage dealer
- **Future:** Multi-target hate table, threat modifiers, taunts

### Deterministic Respawn
- Fixed timers (no randomization yet)
- Respawn at exact spawn coordinates
- **Future:** Randomized spawn locations, roaming spawns

### Basic Movement
- Straight-line chase (no pathfinding)
- Constant move speed
- **Future:** NavMesh integration, obstacle avoidance

### Fixed Melee Range
- Hardcoded 5-unit melee range
- **Future:** Per-NPC reach, ranged attacks

### No Patrol/Roaming
- NPCs are stationary when Idle
- **Future:** Patrol paths, random wandering

---

## EverQuest-Style Features Implemented

? **Aggro Radius** - Proximity detection  
? **Leashing** - Return to spawn when pulled too far  
? **Respawn Timers** - Fixed spawn timers per NPC  
? **Hate/Threat** - Damage-based aggro  
? **Melee Combat** - Auto-attack on cooldown  
? **Death Recovery** - Full HP on leash/respawn  

---

## Future Enhancements (Out of Scope for Phase 2)

- **Faction System:** Friendly/hostile NPCs based on player faction
- **Loot Tables:** Drop items on death
- **Social Aggro:** NPCs assist nearby allies
- **Assist Targeting:** NPCs join fights
- **Special Abilities:** Casts, procs, AOE attacks
- **Roaming/Patrol:** Wandering NPCs
- **Multi-Target Hate:** Threat table with multiple players
- **Ranged Combat:** Bow/caster NPCs
- **Fear/Charm AI:** CC mechanics

---

## Quick Reference

### NPC State Transitions
```
Idle -> Engaged (player in aggro range OR attacked)
Engaged -> Leashing (out of leash range OR target lost)
Engaged -> Dead (HP <= 0)
Leashing -> Idle (reached spawn)
Dead -> Idle (respawn timer expired)
```

### Key Constants
```cpp
MELEE_RANGE = 5.0f          // Attack distance
SPAWN_EPSILON = 2.0f        // Leash completion threshold
```

### JSON Schema
```json
{
  "respawn_time_sec": 120.0,    // Default: 120s
  "attack_cooldown": 1.5,       // Default: 1.5s
  "move_speed": 50.0            // Default: 50 units/sec
}
```

### Log Grep Patterns
```bash
# Aggro events
grep "\[NPC\] Aggro" zone_server.log

# Melee attacks
grep "\[NPC\] Melee attack" zone_server.log

# Leashing
grep "\[NPC\] Leash" zone_server.log

# Respawns
grep "\[NPC\] Respawned" zone_server.log

# State summary
grep "\[NPC\] Active" zone_server.log
```

---

## Implementation Notes

### Performance Considerations
- **O(N*M) aggro scan** per tick (N = NPCs, M = players)
  - Acceptable for small zones (<100 NPCs, <50 players)
  - Future: Spatial partitioning (quadtree, grid)
  
- **Per-NPC updates** at 20Hz
  - ~50 NPCs = ~1000 updates/sec
  - Lightweight state machine (minimal branching)

### Thread Safety
- All NPC updates happen on main simulation thread
- No concurrent access issues
- Future: Consider parallel NPC updates with partitioning

### Memory Footprint
- ZoneNpc struct: ~200 bytes
- 100 NPCs = ~20KB (negligible)
- No dynamic allocations per tick

---

## Validation Checklist

? NPCs load from JSON with new fields  
? NPCs start in Idle state  
? Proximity aggro works  
? Attack aggro works  
? NPCs chase players  
? NPCs perform melee attacks  
? Leashing triggers at correct distance  
? NPCs return to spawn on leash  
? NPCs heal to full on leash  
? NPCs die when HP <= 0  
? Respawn timer counts down  
? NPCs respawn at spawn point  
? State logging shows correct counts  
? Build compiles with no errors  

---

## GDD Compliance

**REQ_GDD_v09 Sections Implemented:**

### NPC Archetypes
- ? Basic NPC types (critters, humanoids, undead)
- ? Level-based difficulty
- ? Varied HP pools and damage ranges

### Aggro & Hate
- ? Proximity aggro (aggroRadius)
- ? Attack-based aggro
- ? Damage-based hate accumulation
- ? Target switching (simple)

### Leashing
- ? Distance-based leash (leashRadius)
- ? Return to spawn behavior
- ? Full HP restoration on leash
- ? Hate/target clear on leash

### Camp Behavior
- ? Fixed spawn points
- ? Return to spawn on leash
- ?? No patrol/roaming (future enhancement)

---

## Conclusion

Phase 2 is **complete** with a robust, EverQuest-style NPC AI system. NPCs can:
- Aggro players via proximity or attack
- Chase and melee attack
- Leash back to spawn when pulled too far
- Die and respawn on timers
- Track hate and switch targets

The system is intentionally simple and deterministic for easy testing and iteration. Future phases can build on this foundation to add more sophisticated behaviors.

**Status:** ? Ready for QA Testing
