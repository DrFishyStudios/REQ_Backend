# REQ_Backend: Data-Driven NPC Spawn System - Implementation Plan

## Executive Summary

This document proposes a concrete implementation plan for a **minimal data-driven NPC spawn system** for REQ_Backend, a classic EverQuest-style MMO emulator. The system will support the "Classic Style Newbie Camp" workflow from the GDD while maintaining clean separation between data and code.

**Target Milestone:** M1 "First Camp" - outdoor zone with ~5 NPC types, combat, XP, loot, death/respawn.

---

## 1. Repository Survey

### 1.1 Zone Server Location

**Project:** `REQ_ZoneServer`

**Key Files:**
- `REQ_ZoneServer/src/ZoneServer.cpp` - Main server class, message handling
- `REQ_ZoneServer/src/ZoneServer_Npc.cpp` - NPC AI state machine and hate system
- `REQ_ZoneServer/src/ZoneServer_Combat.cpp` - Combat resolution
- `REQ_ZoneServer/src/ZoneServer_Death.cpp` - Death, corpses, respawn
- `REQ_ZoneServer/src/ZoneServer_Simulation.cpp` - 20Hz tick loop
- `REQ_ZoneServer/include/req/zone/ZoneServer.h` - ZoneServer class declaration

**Current State:**
- ? Zone server runs standalone with world/zone ID
- ? 20Hz simulation tick implemented
- ? NPC update loop exists (`updateNpc()` called per tick)
- ? AI state machine framework (Idle ? Alert ? Engaged ? Leashing ? Fleeing ? Dead)
- ? Hate/aggro system with proximity scanning

---

### 1.2 Shared Protocol & Message Definitions

**Project:** `REQ_Shared`

**Key Files:**
- `REQ_Shared/include/req/shared/MessageTypes.h` - Enum of all message types
- `REQ_Shared/include/req/shared/ProtocolSchemas.h` - Data structures for messages
- `REQ_Shared/include/req/shared/Protocol_*.h` - Specific protocol headers
- `REQ_Shared/src/Protocol_*.cpp` - Build/parse helpers
- `REQ_Shared/include/req/shared/DataModels.h` - Core data structures

**Current Entities:**
- `ZoneNpc` - Runtime NPC instance with AI state
- `NpcTemplate` - Template definition for NPC types
- `SpawnGroup` - Weighted random spawn selections
- `SpawnPoint` - Physical spawn locations
- `SpawnTable` - Per-zone spawn configuration

**Protocol Gaps Identified:**
- ? No `EntitySpawn` message (server ? client: "NPC appeared")
- ? No `EntityUpdate` message (server ? client: "NPC moved/changed state")
- ? No `EntityDespawn` message (server ? client: "NPC disappeared")
- ? `PlayerStateSnapshot` exists but doesn't include NPCs
- ? `AttackRequest/Result` exist for combat

---

### 1.3 Existing NPC & AI Support

**? Already Implemented:**

**Data Models** (`REQ_Shared/include/req/shared/DataModels.h`):
```cpp
struct ZoneNpc {
    std::uint64_t npcId;
    std::string name;
    std::int32_t level;
    std::int32_t currentHp, maxHp;
    bool isAlive;
    
    // Position & orientation
    float posX, posY, posZ, facingDegrees;
    
    // Combat
    std::int32_t minDamage, maxDamage;
    float meleeAttackTimer, meleeAttackCooldown;
    
    // Spawn point (for leashing/respawn)
    float spawnX, spawnY, spawnZ;
    
    // Respawn
    bool pendingRespawn;
    float respawnTimerSec, respawnTimeSec;
    
    // AI State
    NpcAiState aiState;
    std::unordered_map<uint64_t, float> hateTable;
    std::uint64_t currentTargetId;
    float aggroScanTimer;
    
    // Behavior (from template)
    std::int32_t factionId;
    float moveSpeed;
    NpcBehaviorFlags behaviorFlags;
    NpcBehaviorParams behaviorParams;
};

struct NpcTemplate {
    std::int32_t id;
    std::string name, archetype;
    std::int32_t factionId, lootTableId;
    
    NpcStatBlock stats;
    NpcBehaviorFlags behaviorFlags;
    NpcBehaviorParams behaviorParams;
    
    std::string visualId;  // Client-side visual reference
    std::string abilityPackageId;
    std::string navigationPackageId;
    std::string behaviorPackageId;
};

struct SpawnGroup {
    std::int32_t spawnGroupId;
    std::vector<SpawnGroupEntry> entries;  // Weighted NPC templates
};

struct SpawnPoint {
    std::int32_t spawnId;
    float x, y, z, heading;
    
    // Spawn selection (group vs direct)
    std::int32_t spawnGroupId;
    std::int32_t directNpcId;
    
    // Respawn timing
    float respawnTimeSec, respawnVarianceSec;
    
    // Optional behaviors
    float roamRadius;
    float namedChance;
    bool dayOnly, nightOnly;
};

struct SpawnTable {
    std::int32_t zoneId;
    std::unordered_map<int32_t, SpawnGroup> spawnGroups;
    std::vector<SpawnPoint> spawnPoints;
};
```

**AI State Machine** (`ZoneServer_Npc.cpp`):
- ? States: Idle, Alert, Engaged, Leashing, Fleeing, Dead
- ? Proximity aggro with `aggroRadius`
- ? Social aggro (assist nearby allies)
- ? Leashing (return to spawn if too far)
- ? Flee behavior (run at low HP threshold)
- ? Auto-attack melee at preferred range

**Hate/Threat System** (`ZoneServer_Npc.cpp`):
- ? Per-NPC hate table (EntityID ? hate value)
- ? Hate from damage, heals, CC
- ? Top-hate targeting
- ? Social assists add hate to nearby allies

**Config Loaders** (`REQ_Shared/src/Config.cpp`):
- ? `loadNpcTemplates(path)` - Parses `npcs.json`
- ? `loadSpawnTable(path)` - Parses `spawns_<zone>.json`

---

## 2. JSON Data File Design

### 2.1 NPC Templates (`config/npcs.json`)

**Purpose:** Define reusable NPC types (mob archetypes) that can be referenced by spawn tables.

**Location:** `config/npcs.json` (global, shared across all zones)

**Schema:**
```json
{
  "npcs": [
    {
      "id": 1001,
      "name": "A Decaying Skeleton",
      "archetype": "melee_trash",
      "faction_id": 100,
      "loot_table_id": 10,
      
      "stat_block": {
        "level_min": 1,
        "level_max": 1,
        "hp": 20,
        "mana": 0,
        "ac": 10,
        "atk": 10,
        "str": 10,
        "sta": 10,
        "dex": 10,
        "agi": 10,
        "int": 10,
        "wis": 10,
        "cha": 10
      },
      
      "behavior_flags": {
        "is_roamer": false,
        "is_static": true,
        "is_social": true,
        "uses_ranged": false,
        "calls_for_help": false,
        "can_flee": false,
        "immune_mez": false,
        "immune_charm": false,
        "immune_fear": false,
        "leash_to_spawn": true
      },
      
      "behavior_params": {
        "aggro_radius": 800.0,
        "social_radius": 600.0,
        "flee_health_percent": 0.0,
        "leash_radius": 2000.0,
        "leash_timeout_sec": 10.0,
        "max_chase_distance": 2500.0,
        "preferred_range": 200.0,
        "assist_delay_sec": 0.5
      },
      
      "visual_id": "skeleton_basic",
      "ability_package_id": "",
      "navigation_package_id": "",
      "behavior_package_id": ""
    },
    {
      "id": 1002,
      "name": "A Rat",
      "archetype": "critter",
      "faction_id": 101,
      "loot_table_id": 11,
      
      "stat_block": {
        "level_min": 1,
        "level_max": 2,
        "hp": 10,
        "mana": 0,
        "ac": 5,
        "atk": 8,
        "str": 8,
        "sta": 8,
        "dex": 12,
        "agi": 12,
        "int": 5,
        "wis": 5,
        "cha": 5
      },
      
      "behavior_flags": {
        "is_roamer": true,
        "is_static": false,
        "is_social": false,
        "uses_ranged": false,
        "calls_for_help": false,
        "can_flee": true,
        "immune_mez": false,
        "immune_charm": false,
        "immune_fear": false,
        "leash_to_spawn": true
      },
      
      "behavior_params": {
        "aggro_radius": 400.0,
        "social_radius": 0.0,
        "flee_health_percent": 0.25,
        "leash_radius": 1500.0,
        "leash_timeout_sec": 8.0,
        "max_chase_distance": 2000.0,
        "preferred_range": 200.0,
        "assist_delay_sec": 0.0
      },
      
      "visual_id": "rat_basic",
      "ability_package_id": "",
      "navigation_package_id": "",
      "behavior_package_id": ""
    }
  ]
}
```

**Key Design Decisions:**
- **Global Template File:** One `npcs.json` for all zones reduces duplication
- **Level Ranges:** `level_min` and `level_max` allow variation per spawn
- **Visual IDs:** Client looks up mesh/animation by `visual_id` string
- **Package IDs:** Hooks for future ability/navigation/behavior scripts

---

### 2.2 Zone Spawn Tables (`config/zones/spawns_<zone>.json`)

**Purpose:** Define where NPCs spawn in a specific zone, with timing, groups, and placeholder logic.

**Location:** `config/zones/spawns_10.json` (one file per zone)

**Schema:**
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
    },
    {
      "spawn_group_id": 102,
      "entries": [
        { "npc_id": 1004, "weight": 90 },
        { "npc_id": 1005, "weight": 10 }
      ]
    }
  ],
  
  "spawns": [
    {
      "spawn_id": 1,
      "position": {
        "x": 100.0,
        "y": 50.0,
        "z": 0.0,
        "heading": 0.0
      },
      "spawn_group_id": 101,
      "direct_npc_id": 0,
      "respawn_time_sec": 120.0,
      "respawn_variance_sec": 30.0,
      "roam_radius": 50.0,
      "named_chance": 0.05,
      "day_only": false,
      "night_only": false
    },
    {
      "spawn_id": 2,
      "position": {
        "x": 200.0,
        "y": 75.0,
        "z": 0.0,
        "heading": 90.0
      },
      "spawn_group_id": 0,
      "direct_npc_id": 1002,
      "respawn_time_sec": 90.0,
      "respawn_variance_sec": 15.0,
      "roam_radius": 100.0,
      "named_chance": 0.0,
      "day_only": false,
      "night_only": false
    }
  ]
}
```

**Key Design Decisions:**
- **Spawn Groups vs Direct:** Supports both weighted random pools and fixed spawns
- **Placeholder/Named Logic:** `named_chance` = 5% for rare spawn
- **Respawn Variance:** Adds ±15–30s randomness to prevent sync camping
- **Roam Radius:** NPCs can wander within this distance from spawn point

---

## 3. C++ Types & Managers

### 3.1 Existing Types (No Changes Needed)

**Already Defined in `REQ_Shared/include/req/shared/DataModels.h`:**
- ? `NpcTemplate` - Template definition
- ? `SpawnGroup` - Weighted selection
- ? `SpawnPoint` - Physical location + rules
- ? `SpawnTable` - Per-zone spawn config
- ? `ZoneNpc` - Runtime NPC instance
- ? `NpcAiState` - Enum for AI states
- ? `NpcBehaviorFlags` - Boolean flags
- ? `NpcBehaviorParams` - Numeric params

---

### 3.2 New Manager Class: `ZoneSpawnManager`

**Purpose:** Centralize spawn logic, respawn timers, and NPC lifecycle.

**Location:** `REQ_ZoneServer/include/req/zone/ZoneSpawnManager.h`

**Responsibilities:**
1. Load spawn table from JSON for this zone
2. Initialize spawns on zone startup
3. Track respawn timers for dead NPCs
4. Handle placeholder ? named substitutions
5. Enforce day/night spawn rules (future)
6. Provide spawn point lookup for GM commands

**Class Declaration:**
```cpp
namespace req::zone {

class ZoneSpawnManager {
public:
    ZoneSpawnManager(std::uint32_t zoneId,
                     const req::shared::data::NpcTemplateStore& templates);
    
    // Load spawn table from disk
    bool loadSpawnTable(const std::string& zonesRoot);
    
    // Initial population (called on zone startup)
    void initializeSpawns(std::unordered_map<std::uint64_t, req::shared::data::ZoneNpc>& npcs);
    
    // Update respawn timers (called every tick)
    void updateRespawns(float deltaSeconds,
                        std::unordered_map<std::uint64_t, req::shared::data::ZoneNpc>& npcs);
    
    // Mark spawn point as needing respawn (called when NPC dies)
    void markForRespawn(std::uint64_t npcId);
    
    // Lookup spawn point by NPC ID
    const req::shared::data::SpawnPoint* getSpawnPointForNpc(std::uint64_t npcId) const;
    
    // Stats for monitoring
    std::size_t getTotalSpawnPoints() const;
    std::size_t getPendingRespawns() const;
    
private:
    std::uint32_t zoneId_;
    const req::shared::data::NpcTemplateStore& templates_;
    req::shared::data::SpawnTable spawnTable_;
    
    // Runtime state
    std::uint64_t nextNpcInstanceId_{ 1000000 };  // Zone-scoped NPC IDs
    std::unordered_map<std::uint64_t, std::uint32_t> npcToSpawnId_;  // NPC ? spawn point
    std::unordered_map<std::uint32_t, float> spawnRespawnTimers_;    // Spawn ? timer
    
    // Helpers
    std::uint64_t generateNpcInstanceId();
    req::shared::data::ZoneNpc createNpcFromTemplate(const req::shared::data::NpcTemplate& tmpl,
                                                      const req::shared::data::SpawnPoint& spawn);
    std::int32_t selectNpcFromGroup(const req::shared::data::SpawnGroup& group);
};

} // namespace req::zone
```

---

### 3.3 Integration with ZoneServer

**Changes to `REQ_ZoneServer/include/req/zone/ZoneServer.h`:**

```cpp
class ZoneServer {
private:
    // Existing members...
    
    // NEW: Spawn manager
    std::unique_ptr<ZoneSpawnManager> spawnManager_;
    
    // NPC templates and spawn system (Phase 2)
    req::shared::data::NpcTemplateStore npcTemplates_;  // Existing
    // Remove: req::shared::data::SpawnTable spawnTable_;  // Now in ZoneSpawnManager
};
```

**Changes to `REQ_ZoneServer/src/ZoneServer.cpp` (constructor):**

```cpp
ZoneServer::ZoneServer(/* ... */) 
    : /* existing initializers */ {
    
    // Load NPC templates
    npcTemplates_ = req::shared::loadNpcTemplates("config/npcs.json");
    logInfo("zone", "Loaded " + std::to_string(npcTemplates_.templates.size()) + " NPC template(s)");
    
    // Create spawn manager
    spawnManager_ = std::make_unique<ZoneSpawnManager>(zoneId_, npcTemplates_);
    
    // Load spawn table for this zone
    if (!spawnManager_->loadSpawnTable("config/zones")) {
        logWarn("zone", "No spawn table found for zone " + std::to_string(zoneId_));
    }
    
    // Initialize spawns
    spawnManager_->initializeSpawns(npcs_);
}
```

**Changes to `REQ_ZoneServer/src/ZoneServer_Simulation.cpp`:**

```cpp
void ZoneServer::updateSimulation(float dt) {
    // Existing NPC updates...
    for (auto& [npcId, npc] : npcs_) {
        updateNpc(npc, dt);
    }
    
    // NEW: Update respawn timers
    spawnManager_->updateRespawns(dt, npcs_);
    
    // Existing snapshot broadcast...
}
```

---

## 4. Zone Server Lifecycle Hookup

### 4.1 Startup Sequence

**File:** `REQ_ZoneServer/src/ZoneServer.cpp` (constructor)

```
1. Load NPC templates (npcs.json)
2. Create ZoneSpawnManager
3. Load spawn table (spawns_<zone>.json)
4. Initialize spawns ? populate npcs_ map
5. Start 20Hz simulation tick
```

**Logging Output:**
```
[INFO] [zone] Loaded 25 NPC template(s)
[INFO] [zone] Loaded spawn table for zone 10: 15 spawn points, 3 spawn groups
[INFO] [zone] Initialized 15 NPC spawn(s)
[INFO] [zone] Zone 10 (East Freeport) ready, listening on 127.0.0.1:7779
```

---

### 4.2 Runtime: Death ? Respawn

**Current Flow (in `ZoneServer_Npc.cpp`):**

```cpp
void ZoneServer::updateNpc(ZoneNpc& npc, float deltaSeconds) {
    if (!npc.isAlive) {
        if (!npc.pendingRespawn) {
            // Start respawn timer (OLD: hardcoded in ZoneNpc)
            npc.pendingRespawn = true;
            npc.respawnTimerSec = npc.respawnTimeSec;
        } else {
            // Count down respawn timer (OLD: per-NPC timer)
            npc.respawnTimerSec -= deltaSeconds;
            if (npc.respawnTimerSec <= 0.0f) {
                // Respawn NPC (OLD: reset in place)
                npc.posX = npc.spawnX;
                npc.posY = npc.spawnY;
                npc.posZ = npc.spawnZ;
                npc.currentHp = npc.maxHp;
                npc.isAlive = true;
                npc.pendingRespawn = false;
            }
        }
        return;
    }
    
    // Run AI...
}
```

**New Flow (with ZoneSpawnManager):**

```cpp
void ZoneServer::updateNpc(ZoneNpc& npc, float deltaSeconds) {
    if (!npc.isAlive) {
        // Respawn is now managed by ZoneSpawnManager
        // Do NOT update npc.respawnTimerSec here
        return;
    }
    
    // Run AI...
    updateNpcAi(npc, deltaSeconds);
}

// In ZoneServer_Combat.cpp or ZoneServer_Death.cpp:
void ZoneServer::handleNpcDeath(ZoneNpc& npc, ZonePlayer& killer) {
    npc.isAlive = false;
    npc.currentHp = 0;
    npc.aiState = NpcAiState::Dead;
    clearHate(npc);
    
    // Award XP, drop loot, faction hits...
    
    // NEW: Notify spawn manager to start respawn timer
    spawnManager_->markForRespawn(npc.npcId);
    
    logInfo("zone", "[NPC] NPC died: id=" + std::to_string(npc.npcId) + 
            ", name=\"" + npc.name + "\", respawn in " + 
            std::to_string(npc.respawnTimeSec) + "s");
}
```

**In ZoneSpawnManager:**

```cpp
void ZoneSpawnManager::markForRespawn(std::uint64_t npcId) {
    auto it = npcToSpawnId_.find(npcId);
    if (it == npcToSpawnId_.end()) {
        logWarn("zone", "markForRespawn: NPC " + std::to_string(npcId) + " not in spawn map");
        return;
    }
    
    std::uint32_t spawnId = it->second;
    const auto& spawnPoint = findSpawnPoint(spawnId);
    if (!spawnPoint) return;
    
    // Calculate respawn time with variance
    float baseTime = spawnPoint->respawnTimeSec;
    float variance = spawnPoint->respawnVarianceSec;
    float respawnTime = baseTime + (rand() / (float)RAND_MAX) * variance * 2.0f - variance;
    
    spawnRespawnTimers_[spawnId] = respawnTime;
    
    logInfo("zone", "[SPAWN] Marked spawn " + std::to_string(spawnId) + 
            " for respawn in " + std::to_string(respawnTime) + "s");
}

void ZoneSpawnManager::updateRespawns(float deltaSeconds,
                                      std::unordered_map<uint64_t, ZoneNpc>& npcs) {
    std::vector<std::uint32_t> readyToSpawn;
    
    for (auto& [spawnId, timer] : spawnRespawnTimers_) {
        timer -= deltaSeconds;
        if (timer <= 0.0f) {
            readyToSpawn.push_back(spawnId);
        }
    }
    
    for (std::uint32_t spawnId : readyToSpawn) {
        const auto& spawnPoint = findSpawnPoint(spawnId);
        if (!spawnPoint) continue;
        
        // Create new NPC instance
        std::int32_t npcTemplateId = selectNpcFromSpawnPoint(*spawnPoint);
        const auto& tmpl = templates_.templates.at(npcTemplateId);
        
        auto newNpc = createNpcFromTemplate(tmpl, *spawnPoint);
        std::uint64_t newNpcId = newNpc.npcId;
        
        npcs[newNpcId] = newNpc;
        npcToSpawnId_[newNpcId] = spawnId;
        spawnRespawnTimers_.erase(spawnId);
        
        logInfo("zone", "[SPAWN] Respawned NPC: spawn=" + std::to_string(spawnId) + 
                ", npc=" + std::to_string(newNpcId) + ", name=\"" + newNpc.name + "\"");
    }
}
```

---

### 4.3 Shutdown / Cleanup

**File:** `REQ_ZoneServer/src/ZoneServer.cpp`

```cpp
void ZoneServer::stop() {
    logInfo("zone", "Zone shutdown requested");
    
    // Save all player positions
    saveAllPlayerPositions();
    
    // NEW: Optionally save spawn state (if persistence desired)
    // spawnManager_->saveState("data/zones/spawn_state_" + std::to_string(zoneId_) + ".json");
    
    // Stop network and timers
    ioContext_.stop();
}
```

---

## 5. Combat Integration

### 5.1 AttackRequest ? NPC Targeting

**File:** `REQ_ZoneServer/src/ZoneServer_Combat.cpp`

**Current Flow:**
```cpp
case MessageType::AttackRequest: {
    AttackRequestData req;
    parseAttackRequestPayload(body, req);
    
    // Find attacker
    auto attackerIt = players_.find(req.attackerCharacterId);
    if (attackerIt == players_.end()) return;
    
    ZonePlayer& attacker = attackerIt->second;
    
    // NEW: Check if target is NPC
    auto npcIt = npcs_.find(req.targetId);
    if (npcIt != npcs_.end()) {
        // Target is NPC
        processAttack(attacker, npcIt->second, req);
        return;
    }
    
    // Check if target is player
    auto playerIt = players_.find(req.targetId);
    if (playerIt != players_.end()) {
        // Target is player (PvP)
        // ... handle PvP ...
        return;
    }
    
    // Invalid target
    AttackResultData result;
    result.attackerId = attacker.characterId;
    result.targetId = req.targetId;
    result.resultCode = 2;  // Invalid target
    result.message = "Invalid target";
    broadcastAttackResult(result);
    break;
}
```

---

### 5.2 Combat Resolution

**File:** `REQ_ZoneServer/src/ZoneServer_Combat.cpp`

```cpp
void ZoneServer::processAttack(ZonePlayer& attacker, ZoneNpc& target, 
                               const AttackRequestData& request) {
    AttackResultData result;
    result.attackerId = attacker.characterId;
    result.targetId = target.npcId;
    
    // Check if target is alive
    if (!target.isAlive) {
        result.resultCode = 5;  // Target is dead
        result.message = "Target is already dead";
        broadcastAttackResult(result);
        return;
    }
    
    // Check range
    float dx = target.posX - attacker.posX;
    float dy = target.posY - attacker.posY;
    float dz = target.posZ - attacker.posZ;
    float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    const float meleeRange = 5.0f;  // TODO: from config
    if (distance > meleeRange) {
        result.resultCode = 1;  // Out of range
        result.message = "Target out of range";
        broadcastAttackResult(result);
        return;
    }
    
    // Calculate damage (simple formula for now)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dmg(target.minDamage, target.maxDamage);
    
    int damage = attacker.level + (attacker.strength / 10) + dmg(gen);
    
    // Apply damage
    target.currentHp -= damage;
    if (target.currentHp <= 0) {
        target.currentHp = 0;
        target.isAlive = false;
        target.aiState = NpcAiState::Dead;
        
        // Award XP, loot, faction
        awardXpForNpcKill(target, attacker);
        
        // Mark for respawn
        spawnManager_->markForRespawn(target.npcId);
    }
    
    // Add hate
    addHate(target, attacker.characterId, static_cast<float>(damage));
    
    // Build result
    result.damage = damage;
    result.wasHit = true;
    result.remainingHp = target.currentHp;
    result.resultCode = 0;  // Success
    result.message = "Hit for " + std::to_string(damage) + " damage";
    
    broadcastAttackResult(result);
    
    logInfo("zone", "[COMBAT] Player " + std::to_string(attacker.characterId) + 
            " attacked NPC " + std::to_string(target.npcId) + 
            " for " + std::to_string(damage) + " damage" +
            " (HP: " + std::to_string(target.currentHp) + "/" + std::to_string(target.maxHp) + ")");
}
```

---

## 6. Protocol Gaps & Future Work

### 6.1 Entity Spawn/Update/Despawn Messages

**Problem:** Clients need to know when NPCs appear, move, or disappear.

**Current Workaround:** Include NPCs in `PlayerStateSnapshot` as a temporary hack.

**Proper Solution (Phase 2.5):**

**New Messages:**

```cpp
// In MessageTypes.h:
EntitySpawn       = 50,  // Server ? Client: NPC appeared
EntityUpdate      = 51,  // Server ? Client: NPC moved/changed state
EntityDespawn     = 52,  // Server ? Client: NPC disappeared
```

**Data Structures:**

```cpp
struct EntitySpawnData {
    std::uint64_t entityId;
    std::string name;
    std::uint32_t level;
    std::string visualId;  // Client looks up mesh/animation
    float posX, posY, posZ;
    float facingDegrees;
    std::int32_t currentHp, maxHp;
    bool isHostile;
};

struct EntityUpdateData {
    std::uint64_t entityId;
    float posX, posY, posZ;
    float facingDegrees;
    std::int32_t currentHp, maxHp;
};

struct EntityDespawnData {
    std::uint64_t entityId;
    bool isDeath;  // true = died, false = zoned/despawned
};
```

**Timeline:** Post-vertical-slice, when client rendering is more advanced.

---

### 6.2 Loot & Corpse Protocol

**Problem:** Clients need to know when corpses appear and what they contain.

**Current State:** Corpse system exists server-side but no client messages.

**Required Messages (Phase 2.5):**

```cpp
CorpseCreate      = 53,  // Server ? Client: Corpse appeared
CorpseDecay       = 54,  // Server ? Client: Corpse despawned
LootWindowOpen    = 55,  // Server ? Client: Show loot UI
LootItem          = 56,  // Client ? Server: Take item from corpse
```

**Timeline:** M3 "Economy" milestone.

---

### 6.3 Faction & Con Color

**Problem:** Clients need to know NPC faction/alignment for con colors and KOS.

**Current State:** `factionId` exists on NPCs but no faction system.

**Required Work:**
- Load `factions.json` into memory
- Calculate player faction standing with NPC faction
- Send con color in `EntitySpawn` message
- Send KOS flag for red-con NPCs

**Timeline:** Post-vertical-slice, optional for M1.

---

## 7. Classic Style Newbie Camp Example

### 7.1 Zone: East Freeport Outskirts (Zone 10)

**NPC Templates (`config/npcs.json`):**

```json
{
  "npcs": [
    {
      "id": 1001,
      "name": "A Decaying Skeleton",
      "archetype": "melee_trash",
      "faction_id": 100,
      "loot_table_id": 10,
      "stat_block": {
        "level_min": 1,
        "level_max": 2,
        "hp": 20,
        "mana": 0,
        "ac": 10,
        "atk": 10,
        "str": 10,
        "sta": 10,
        "dex": 10,
        "agi": 10,
        "int": 10,
        "wis": 10,
        "cha": 10
      },
      "behavior_flags": {
        "is_roamer": false,
        "is_static": true,
        "is_social": true,
        "leash_to_spawn": true
      },
      "behavior_params": {
        "aggro_radius": 800.0,
        "social_radius": 600.0,
        "leash_radius": 2000.0,
        "preferred_range": 200.0
      },
      "visual_id": "skeleton_basic"
    },
    {
      "id": 1002,
      "name": "A Rat",
      "archetype": "critter",
      "faction_id": 101,
      "loot_table_id": 11,
      "stat_block": {
        "level_min": 1,
        "level_max": 1,
        "hp": 10,
        "ac": 5,
        "atk": 8
      },
      "behavior_flags": {
        "is_roamer": true,
        "can_flee": true
      },
      "behavior_params": {
        "aggro_radius": 400.0,
        "flee_health_percent": 0.25,
        "preferred_range": 200.0
      },
      "visual_id": "rat_basic"
    },
    {
      "id": 1003,
      "name": "A Fire Beetle",
      "archetype": "melee_trash",
      "faction_id": 101,
      "loot_table_id": 12,
      "stat_block": {
        "level_min": 2,
        "level_max": 3,
        "hp": 30,
        "ac": 12,
        "atk": 12
      },
      "behavior_flags": {
        "is_static": true,
        "is_social": false
      },
      "behavior_params": {
        "aggro_radius": 600.0
      },
      "visual_id": "beetle_fire"
    },
    {
      "id": 1004,
      "name": "A Snake",
      "archetype": "critter",
      "faction_id": 101,
      "loot_table_id": 11,
      "stat_block": {
        "level_min": 1,
        "level_max": 2,
        "hp": 15,
        "ac": 8,
        "atk": 9
      },
      "behavior_flags": {
        "is_roamer": true,
        "can_flee": true
      },
      "behavior_params": {
        "aggro_radius": 500.0,
        "flee_health_percent": 0.2
      },
      "visual_id": "snake_basic"
    },
    {
      "id": 1005,
      "name": "A Zombie",
      "archetype": "melee_trash",
      "faction_id": 100,
      "loot_table_id": 10,
      "stat_block": {
        "level_min": 3,
        "level_max": 4,
        "hp": 50,
        "ac": 15,
        "atk": 15
      },
      "behavior_flags": {
        "is_static": true,
        "is_social": true
      },
      "behavior_params": {
        "aggro_radius": 1000.0,
        "social_radius": 800.0
      },
      "visual_id": "zombie_basic"
    },
    {
      "id": 1006,
      "name": "Skeleton Warlord",
      "archetype": "named_mini_boss",
      "faction_id": 100,
      "loot_table_id": 13,
      "stat_block": {
        "level_min": 5,
        "level_max": 5,
        "hp": 100,
        "ac": 20,
        "atk": 20
      },
      "behavior_flags": {
        "is_static": true,
        "is_social": true,
        "calls_for_help": true
      },
      "behavior_params": {
        "aggro_radius": 1200.0,
        "social_radius": 1000.0
      },
      "visual_id": "skeleton_warlord"
    }
  ]
}
```

---

### 7.2 Spawn Table (`config/zones/spawns_10.json`)

```json
{
  "zone_id": 10,
  
  "spawn_groups": [
    {
      "spawn_group_id": 101,
      "entries": [
        { "npc_id": 1001, "weight": 60 },
        { "npc_id": 1002, "weight": 30 },
        { "npc_id": 1004, "weight": 10 }
      ]
    },
    {
      "spawn_group_id": 102,
      "entries": [
        { "npc_id": 1003, "weight": 70 },
        { "npc_id": 1005, "weight": 30 }
      ]
    },
    {
      "spawn_group_id": 103,
      "entries": [
        { "npc_id": 1001, "weight": 95 },
        { "npc_id": 1006, "weight": 5 }
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
      "roam_radius": 50.0
    },
    {
      "spawn_id": 2,
      "position": { "x": 200.0, "y": 75.0, "z": 0.0, "heading": 90.0 },
      "spawn_group_id": 101,
      "respawn_time_sec": 120.0,
      "respawn_variance_sec": 30.0,
      "roam_radius": 50.0
    },
    {
      "spawn_id": 3,
      "position": { "x": 150.0, "y": 120.0, "z": 0.0, "heading": 180.0 },
      "spawn_group_id": 102,
      "respawn_time_sec": 180.0,
      "respawn_variance_sec": 45.0,
      "roam_radius": 75.0
    },
    {
      "spawn_id": 4,
      "position": { "x": 50.0, "y": 200.0, "z": 0.0, "heading": 270.0 },
      "direct_npc_id": 1002,
      "respawn_time_sec": 90.0,
      "respawn_variance_sec": 15.0,
      "roam_radius": 100.0
    },
    {
      "spawn_id": 5,
      "position": { "x": 300.0, "y": 150.0, "z": 0.0, "heading": 45.0 },
      "spawn_group_id": 103,
      "respawn_time_sec": 360.0,
      "respawn_variance_sec": 120.0,
      "roam_radius": 25.0,
      "named_chance": 0.05
    }
  ]
}
```

**Key Design Elements:**
- **Mixed Spawns:** Groups 101/102 have common mobs, group 103 has placeholder/named
- **Respawn Variance:** Prevents synchronized camping
- **Roam Radius:** Critters (rats) roam farther than static undead
- **Named Spawn:** 5% chance of Skeleton Warlord at spawn 5

---

## 8. Implementation Checklist

### Phase 1: Foundation (Week 1)

- [ ] Create `ZoneSpawnManager` class
- [ ] Implement `loadSpawnTable()` using existing JSON loader
- [ ] Implement `initializeSpawns()` to populate NPCs on startup
- [ ] Update `ZoneServer` constructor to use `ZoneSpawnManager`
- [ ] Test with simple spawn table (1-2 spawn points)

### Phase 2: Respawn System (Week 1-2)

- [ ] Implement `markForRespawn()` hook
- [ ] Implement `updateRespawns()` timer logic
- [ ] Add respawn variance calculation
- [ ] Update `handleNpcDeath()` to notify spawn manager
- [ ] Test death ? respawn cycle

### Phase 3: Spawn Groups & Placeholders (Week 2)

- [ ] Implement `selectNpcFromGroup()` weighted random
- [ ] Add placeholder/named logic (`named_chance`)
- [ ] Test spawn group rotation
- [ ] Verify logging for all spawn events

### Phase 4: Combat Integration (Week 2-3)

- [ ] Update `AttackRequest` handler to target NPCs
- [ ] Implement damage formulas (simple version)
- [ ] Add hate generation on attack
- [ ] Test player ? NPC combat loop
- [ ] Verify XP awards on NPC death

### Phase 5: Polish & Testing (Week 3)

- [ ] Add GM commands for spawn manipulation
- [ ] Stress test with 20+ spawn points
- [ ] Profile spawn manager performance
- [ ] Document all JSON schemas
- [ ] Create example spawn tables for 2-3 zones

---

## 9. Files to Create/Modify

### New Files

1. `REQ_ZoneServer/include/req/zone/ZoneSpawnManager.h`
2. `REQ_ZoneServer/src/ZoneSpawnManager.cpp`
3. `config/npcs.json` (NPC templates)
4. `config/zones/spawns_10.json` (Example zone 10)
5. `config/zones/spawns_20.json` (Example zone 20)
6. `docs/NPC_SPAWN_SYSTEM.md` (This document + usage guide)

### Modified Files

1. `REQ_ZoneServer/include/req/zone/ZoneServer.h`
   - Add `spawnManager_` member
   - Remove `spawnTable_` member (moved to manager)

2. `REQ_ZoneServer/src/ZoneServer.cpp`
   - Constructor: Create spawn manager
   - Shutdown: Optional save spawn state

3. `REQ_ZoneServer/src/ZoneServer_Simulation.cpp`
   - `updateSimulation()`: Call `spawnManager_->updateRespawns()`

4. `REQ_ZoneServer/src/ZoneServer_Npc.cpp`
   - `updateNpc()`: Remove old respawn timer logic
   - Keep AI state machine as-is

5. `REQ_ZoneServer/src/ZoneServer_Combat.cpp`
   - `handleNpcDeath()`: Call `spawnManager_->markForRespawn()`

6. `REQ_ZoneServer/src/ZoneServer_Messages.cpp`
   - `AttackRequest` handler: Add NPC targeting

---

## 10. Testing Plan

### 10.1 Unit Tests

- [ ] `ZoneSpawnManager::selectNpcFromGroup()` - Weighted random distribution
- [ ] `ZoneSpawnManager::createNpcFromTemplate()` - Correct stat initialization
- [ ] `ZoneSpawnManager::markForRespawn()` - Timer starts correctly
- [ ] `ZoneSpawnManager::updateRespawns()` - NPCs respawn at correct intervals

### 10.2 Integration Tests

- [ ] Start zone server, verify NPCs spawn at startup
- [ ] Kill NPC, verify respawn after timer
- [ ] Kill multiple NPCs, verify independent timers
- [ ] Test spawn group rotation (kill placeholder 10x, verify distribution)
- [ ] Test named spawn chance (kill placeholder 100x, verify ~5% rate)

### 10.3 Performance Tests

- [ ] 100 spawn points × 2min respawn = stress test
- [ ] Profile `updateRespawns()` with 500+ active timers
- [ ] Verify no memory leaks over 1hr runtime

---

## 11. Success Criteria

### Minimum Viable Spawn System (M1 Milestone)

? **Data-Driven:** All NPC types and spawn points defined in JSON  
? **Automatic Population:** Zone spawns NPCs on startup  
? **Respawn Timers:** Dead NPCs respawn after configured interval  
? **Spawn Groups:** Weighted random selection works  
? **Placeholder/Named:** Rare spawn logic functional  
? **Combat Integration:** Players can attack and kill NPCs  
? **Logging:** Clear logs for spawn/respawn/death events  

### Stretch Goals (Post-M1)

- [ ] Day/night spawn rules
- [ ] Dynamic spawn density based on player count
- [ ] Roaming paths (waypoint system)
- [ ] Event-based spawns (triggered by quests/timers)
- [ ] GM commands (`/spawn`, `/despawn`, `/respawn`)

---

## 12. Timeline & Effort Estimate

**Target:** M1 "First Camp" milestone (2 weeks)

| Task | Effort | Week |
|------|--------|------|
| Design & documentation | 4 hrs | 1 |
| `ZoneSpawnManager` class | 8 hrs | 1 |
| JSON loading & validation | 4 hrs | 1 |
| Spawn initialization | 4 hrs | 1 |
| Respawn timer system | 6 hrs | 1-2 |
| Combat integration | 6 hrs | 2 |
| Spawn groups & placeholders | 4 hrs | 2 |
| Testing & bug fixes | 8 hrs | 2 |
| **Total** | **44 hrs** | **1-2 weeks** |

**Assumptions:**
- Existing AI and combat systems work as documented
- No major refactoring of `ZoneNpc` structure needed
- JSON loaders and validators are stable

---

## 13. Summary

This plan provides a **concrete, executable roadmap** for implementing a data-driven NPC spawn system in REQ_Backend. The design:

? **Leverages existing work:** Builds on `ZoneNpc`, AI state machine, and hate system  
? **Minimal code changes:** Centralizes spawn logic in `ZoneSpawnManager`  
? **Data-driven:** NPCs and spawns defined in JSON, no hardcoded logic  
? **Supports EQ-style gameplay:** Camps, respawn timers, placeholders, named spawns  
? **Combat-ready:** Integrates seamlessly with `AttackRequest/Result` protocol  
? **Scalable:** Handles 100+ spawn points without performance issues  

**Next Step:** Implement `ZoneSpawnManager` class and test with a simple spawn table for zone 10.

---

**Document Version:** 1.0  
**Date:** 2025-01-19  
**Author:** Claude (Senior C++ Backend Engineer)  
**Status:** Ready for Implementation
