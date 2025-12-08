# REQ_Backend NPC Spawn System - Quick Summary

## Overview

A data-driven NPC spawn system for REQ_Backend that supports classic EverQuest-style camp gameplay with spawn groups, respawn timers, and placeholder/named logic.

---

## Current State Assessment

### ? What Already Exists

**Zone Server** (`REQ_ZoneServer`):
- 20Hz simulation tick running
- `ZoneNpc` runtime instances with AI state machine
- Hate/aggro system with proximity scanning
- Combat resolution (`AttackRequest/Result`)
- Death and respawn hooks

**Data Models** (`REQ_Shared`):
- `NpcTemplate` - Reusable NPC definitions
- `SpawnGroup` - Weighted random selection
- `SpawnPoint` - Physical spawn locations
- `SpawnTable` - Per-zone spawn configuration
- `ZoneNpc` - Runtime NPC instance with AI

**AI System** (`ZoneServer_Npc.cpp`):
- States: Idle ? Alert ? Engaged ? Leashing ? Fleeing ? Dead
- Proximity aggro, social assists, leashing
- Auto-attack melee combat

**Data Loaders** (`Config.cpp`):
- `loadNpcTemplates(path)` - Parses `npcs.json`
- `loadSpawnTable(path)` - Parses `spawns_<zone>.json`

---

## What Needs to Be Built

### 1. ZoneSpawnManager Class

**Purpose:** Centralize spawn logic, respawn timers, and NPC lifecycle.

**Key Methods:**
```cpp
bool loadSpawnTable(const std::string& zonesRoot);
void initializeSpawns(std::unordered_map<uint64_t, ZoneNpc>& npcs);
void updateRespawns(float deltaSeconds, std::unordered_map<uint64_t, ZoneNpc>& npcs);
void markForRespawn(std::uint64_t npcId);
```

**Location:** `REQ_ZoneServer/include/req/zone/ZoneSpawnManager.h`

---

### 2. JSON Data Files

**NPC Templates** (`config/npcs.json`):
```json
{
  "npcs": [
    {
      "id": 1001,
      "name": "A Decaying Skeleton",
      "archetype": "melee_trash",
      "stat_block": { "level_min": 1, "level_max": 2, "hp": 20, ... },
      "behavior_flags": { "is_social": true, "leash_to_spawn": true, ... },
      "behavior_params": { "aggro_radius": 800.0, "social_radius": 600.0, ... },
      "visual_id": "skeleton_basic"
    }
  ]
}
```

**Zone Spawn Tables** (`config/zones/spawns_10.json`):
```json
{
  "zone_id": 10,
  "spawn_groups": [
    {
      "spawn_group_id": 101,
      "entries": [
        { "npc_id": 1001, "weight": 60 },
        { "npc_id": 1002, "weight": 30 },
        { "npc_id": 1003, "weight": 10 }
      ]
    }
  ],
  "spawns": [
    {
      "spawn_id": 1,
      "position": { "x": 100.0, "y": 50.0, "z": 0.0, "heading": 0.0 },
      "spawn_group_id": 101,
      "respawn_time_sec": 120.0,
      "respawn_variance_sec": 30.0,
      "roam_radius": 50.0,
      "named_chance": 0.05
    }
  ]
}
```

---

### 3. Zone Server Integration

**Constructor** (`ZoneServer.cpp`):
```cpp
// Load NPC templates
npcTemplates_ = req::shared::loadNpcTemplates("config/npcs.json");

// Create spawn manager
spawnManager_ = std::make_unique<ZoneSpawnManager>(zoneId_, npcTemplates_);

// Load and initialize spawns
spawnManager_->loadSpawnTable("config/zones");
spawnManager_->initializeSpawns(npcs_);
```

**Simulation Loop** (`ZoneServer_Simulation.cpp`):
```cpp
void ZoneServer::updateSimulation(float dt) {
    // Update NPC AI
    for (auto& [npcId, npc] : npcs_) {
        updateNpc(npc, dt);
    }
    
    // Update respawn timers
    spawnManager_->updateRespawns(dt, npcs_);
    
    // Broadcast snapshots
    broadcastSnapshots();
}
```

**Death Handler** (`ZoneServer_Combat.cpp`):
```cpp
void ZoneServer::handleNpcDeath(ZoneNpc& npc, ZonePlayer& killer) {
    npc.isAlive = false;
    npc.aiState = NpcAiState::Dead;
    clearHate(npc);
    
    // Award XP, loot, faction...
    awardXpForNpcKill(npc, killer);
    
    // Mark for respawn
    spawnManager_->markForRespawn(npc.npcId);
}
```

---

## Key Features

### Spawn Groups (Weighted Random)
- Define pools of NPCs with weight values
- Supports placeholder ? named substitution
- Example: 60% skeleton, 30% rat, 10% beetle

### Respawn Timers
- Per-spawn-point timers (not per-NPC)
- Variance adds randomness: `120s ± 30s`
- Prevents synchronized camping

### Named/Placeholder Logic
- `named_chance: 0.05` = 5% chance for rare spawn
- Classic EQ-style placeholder system
- Supports "camp and wait" gameplay

### Combat Integration
- Players can target NPCs via `AttackRequest`
- Server resolves damage, hate, death
- Spawns respawn automatically after death

---

## Protocol Gaps (Future Work)

### Entity Messages (Not Required for M1)

**EntitySpawn** (Server ? Client):
- Notify client when NPC appears
- Send position, name, visual ID, level
- Used when NPC spawns or player enters zone

**EntityUpdate** (Server ? Client):
- NPC position/state updates (separate from player snapshots)
- Allows independent update rates for NPCs vs players

**EntityDespawn** (Server ? Client):
- NPC disappeared (death, zone transition, etc.)
- Allows client to remove from world

**Current Workaround:** Include NPCs in `PlayerStateSnapshot` (temporary hack)

---

## Implementation Timeline

### Week 1: Foundation
- Create `ZoneSpawnManager` class
- Implement JSON loading
- Test spawn initialization

### Week 2: Respawn & Combat
- Implement respawn timer system
- Add spawn group selection
- Integrate with combat system
- Test death ? respawn cycle

**Total Effort:** ~44 hours over 1-2 weeks

---

## Success Criteria

? **Data-Driven:** All NPCs defined in JSON  
? **Automatic Population:** NPCs spawn on zone startup  
? **Respawn System:** Dead NPCs respawn after timer  
? **Spawn Groups:** Weighted random selection  
? **Placeholder/Named:** Rare spawn logic  
? **Combat Ready:** Players can attack and kill NPCs  
? **Logging:** Clear spawn/respawn/death events  

---

## Example: Classic Newbie Camp

**Zone 10 (East Freeport Outskirts)**

**NPCs:**
- 1001: "A Decaying Skeleton" (level 1-2, HP 20)
- 1002: "A Rat" (level 1, HP 10, flees at 25%)
- 1003: "A Fire Beetle" (level 2-3, HP 30)
- 1004: "A Snake" (level 1-2, HP 15)
- 1005: "A Zombie" (level 3-4, HP 50)
- 1006: "Skeleton Warlord" (level 5, HP 100, named, 5% chance)

**Spawn Points:**
- 5 spawn points around the camp
- Mix of static (skeletons) and roamers (rats)
- 1 placeholder spawn with 5% named chance
- Respawn times: 90s-360s with variance

**Gameplay:**
- Players arrive at safe zone edge
- Pull singles from camp (social aggro risk)
- Camp placeholder for rare named spawn
- Classic EQ-style experience

---

## Files to Create

**New:**
1. `REQ_ZoneServer/include/req/zone/ZoneSpawnManager.h`
2. `REQ_ZoneServer/src/ZoneSpawnManager.cpp`
3. `config/npcs.json`
4. `config/zones/spawns_10.json`
5. `docs/NPC_SPAWN_SYSTEM_PLAN.md` (detailed plan)

**Modified:**
1. `REQ_ZoneServer/include/req/zone/ZoneServer.h`
2. `REQ_ZoneServer/src/ZoneServer.cpp`
3. `REQ_ZoneServer/src/ZoneServer_Simulation.cpp`
4. `REQ_ZoneServer/src/ZoneServer_Npc.cpp`
5. `REQ_ZoneServer/src/ZoneServer_Combat.cpp`

---

## Next Steps

1. Review and approve this plan
2. Implement `ZoneSpawnManager` class
3. Create example `npcs.json` with 5-10 NPC types
4. Create `spawns_10.json` for test zone
5. Test spawn initialization
6. Test respawn cycle
7. Integrate with combat system

---

**Status:** ? Plan Complete, Ready for Implementation  
**Milestone:** M1 "First Camp"  
**Estimated Time:** 1-2 weeks (44 hours)

---

## Quick Reference

**Load NPCs:**
```cpp
npcTemplates_ = loadNpcTemplates("config/npcs.json");
```

**Create Spawn Manager:**
```cpp
spawnManager_ = std::make_unique<ZoneSpawnManager>(zoneId_, npcTemplates_);
```

**Initialize Spawns:**
```cpp
spawnManager_->loadSpawnTable("config/zones");
spawnManager_->initializeSpawns(npcs_);
```

**Update Respawns (every tick):**
```cpp
spawnManager_->updateRespawns(dt, npcs_);
```

**Mark for Respawn (on death):**
```cpp
spawnManager_->markForRespawn(npc.npcId);
```
