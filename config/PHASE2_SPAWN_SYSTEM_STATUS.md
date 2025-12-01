# Phase 2: Spawn System & NPC Templates - Implementation Status

## ? Completed Steps

### 1. Data Structures (DataModels.h)
- ? NpcBehaviorFlags struct
- ? NpcBehaviorParams struct
- ? NpcStatBlock struct
- ? NpcTemplate struct
- ? NpcTemplateStore struct
- ? SpawnGroupEntry struct
- ? SpawnGroup struct
- ? SpawnPoint struct
- ? SpawnTable struct

### 2. Loader Functions (Config.h/cpp)
- ? `loadNpcTemplates()` declaration
- ? `loadSpawnTable()` declaration
- ? `loadNpcTemplates()` implementation with full JSON parsing
- ? `loadSpawnTable()` implementation with spawn groups and validation

### 3. Sample Data Files
- ? `data/npcs.json` - 6 NPC templates (skeletons, rats, beetles, gnolls, kobold shaman)
- ? `data/spawns_10.json` - Zone 10 spawn table with 6 spawn points, 3 spawn groups
- ? `data/spawns_20.json` - Zone 20 spawn table with 5 spawn points, 3 spawn groups

### 4. ZoneServer Integration (Partial)
- ? Added `npcTemplates_` member to ZoneServer.h
- ? Added `spawnTable_` member to ZoneServer.h

---

## ?? Remaining Work

### Step 7: Load NPC Templates in ZoneServer Constructor

**File:** `REQ_ZoneServer/src/ZoneServer.cpp`  
**Location:** In `ZoneServer::ZoneServer()` constructor, after loading zone config

**Add this code:**
```cpp
// Load NPC templates (Phase 2)
try {
    npcTemplates_ = req::shared::loadNpcTemplates("data/npcs.json");
    req::shared::logInfo("zone", std::string{"Loaded "} + 
        std::to_string(npcTemplates_.templates.size()) + " NPC template(s)");
} catch (const std::exception& e) {
    req::shared::logError("zone", std::string{"Failed to load NPC templates: "} + e.what());
    // Non-fatal - zone can run without templates
}
```

---

### Step 8: Load Spawn Table in ZoneServer::run()

**File:** `REQ_ZoneServer/src/ZoneServer.cpp`  
**Function:** `void ZoneServer::run()`  
**Location:** Replace the call to `loadNpcsForZone()` with spawn table loading

**Replace this:**
```cpp
// Load NPCs for this zone
loadNpcsForZone();
```

**With this:**
```cpp
// Load spawn table for this zone (Phase 2)
try {
    std::string spawnTablePath = "data/spawns_" + std::to_string(zoneId_) + ".json";
    spawnTable_ = req::shared::loadSpawnTable(spawnTablePath);
    req::shared::logInfo("zone", std::string{"Loaded spawn table: "} + 
        std::to_string(spawnTable_.spawnPoints.size()) + " spawn point(s), " +
        std::to_string(spawnTable_.spawnGroups.size()) + " spawn group(s)");
} catch (const std::exception& e) {
    req::shared::logWarn("zone", std::string{"Failed to load spawn table: "} + e.what() + 
        ", zone will have no spawns");
}

// Instantiate NPCs from spawn points
instantiateNpcsFromSpawnTable();
```

---

### Step 9: Implement instantiateNpcsFromSpawnTable()

**File:** `REQ_ZoneServer/src/ZoneServer_Npc.cpp`  
**Location:** Add new function after `loadNpcsForZone()`

**Add declaration to ZoneServer.h:**
```cpp
// In private section
void instantiateNpcsFromSpawnTable();
```

**Implementation:**
```cpp
void ZoneServer::instantiateNpcsFromSpawnTable() {
    if (spawnTable_.spawnPoints.empty()) {
        req::shared::logInfo("zone", "[SPAWN] No spawn points defined for this zone");
        return;
    }

    static std::uint64_t nextNpcInstanceId = 1;  // Global NPC instance counter
    int spawnedCount = 0;

    for (const auto& spawnPoint : spawnTable_.spawnPoints) {
        // Determine which NPC template to use
        std::int32_t npcTemplateId = 0;

        if (spawnPoint.spawnGroupId != 0) {
            // Use spawn group - weighted random selection
            auto groupIt = spawnTable_.spawnGroups.find(spawnPoint.spawnGroupId);
            if (groupIt == spawnTable_.spawnGroups.end()) {
                req::shared::logWarn("zone", std::string{"[SPAWN] Spawn point "} + 
                    std::to_string(spawnPoint.spawnId) + " references invalid spawn_group_id: " +
                    std::to_string(spawnPoint.spawnGroupId));
                continue;
            }

            // Weighted random selection
            const auto& group = groupIt->second;
            int totalWeight = 0;
            for (const auto& entry : group.entries) {
                totalWeight += entry.weight;
            }

            if (totalWeight <= 0) {
                req::shared::logWarn("zone", std::string{"[SPAWN] Spawn group "} +
                    std::to_string(spawnPoint.spawnGroupId) + " has zero total weight");
                continue;
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, totalWeight);
            int roll = dis(gen);

            int cumulative = 0;
            for (const auto& entry : group.entries) {
                cumulative += entry.weight;
                if (roll <= cumulative) {
                    npcTemplateId = entry.npcId;
                    break;
                }
            }
        } else if (spawnPoint.directNpcId != 0) {
            // Use direct NPC ID
            npcTemplateId = spawnPoint.directNpcId;
        } else {
            req::shared::logWarn("zone", std::string{"[SPAWN] Spawn point "} +
                std::to_string(spawnPoint.spawnId) + " has neither spawn_group_id nor direct_npc_id");
            continue;
        }

        // Look up NPC template
        auto templateIt = npcTemplates_.templates.find(npcTemplateId);
        if (templateIt == npcTemplates_.templates.end()) {
            req::shared::logWarn("zone", std::string{"[SPAWN] Spawn point "} +
                std::to_string(spawnPoint.spawnId) + " references unknown NPC template: " +
                std::to_string(npcTemplateId));
            continue;
        }

        const auto& npcTemplate = templateIt->second;

        // Create runtime NPC instance from template
        req::shared::data::ZoneNpc npc;
        npc.npcId = nextNpcInstanceId++;
        npc.name = npcTemplate.name;

        // Randomize level within template range
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> levelDist(npcTemplate.stats.levelMin, npcTemplate.stats.levelMax);
        npc.level = levelDist(gen);

        // Set stats from template (TODO: scale by level)
        npc.maxHp = npcTemplate.stats.hp;
        npc.currentHp = npc.maxHp;
        npc.isAlive = true;
        npc.minDamage = 1;  // TODO: calculate from template stats
        npc.maxDamage = npc.level * 2;  // TODO: calculate from template stats

        // Position from spawn point
        npc.posX = spawnPoint.x;
        npc.posY = spawnPoint.y;
        npc.posZ = spawnPoint.z;
        npc.facingDegrees = spawnPoint.heading;

        // Store spawn point for respawn
        npc.spawnX = spawnPoint.x;
        npc.spawnY = spawnPoint.y;
        npc.spawnZ = spawnPoint.z;

        // Behavior params from template
        npc.aggroRadius = npcTemplate.behaviorParams.aggroRadius / 10.0f;  // Convert to unit scale
        npc.leashRadius = npcTemplate.behaviorParams.leashRadius / 10.0f;  // Convert to unit scale
        npc.moveSpeed = 50.0f;  // TODO: from template or navigation package

        // Respawn timing from spawn point
        npc.respawnTimeSec = spawnPoint.respawnTimeSec;
        npc.respawnTimerSec = 0.0f;
        npc.pendingRespawn = false;

        // AI state
        npc.state = req::shared::data::ZoneNpc::AiState::Idle;
        npc.currentTargetId = 0;
        npc.hateValue = 0.0f;
        npc.attackCooldown = 1.5f;  // TODO: from template or ability package
        npc.attackTimer = 0.0f;

        // Add to zone
        npcs_[npc.npcId] = npc;
        spawnedCount++;

        req::shared::logInfo("zone", std::string{"[SPAWN] Spawned NPC: id="} +
            std::to_string(npc.npcId) + ", template=" + std::to_string(npcTemplateId) +
            ", name=\"" + npc.name + "\", level=" + std::to_string(npc.level) +
            ", spawnId=" + std::to_string(spawnPoint.spawnId) +
            ", pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," +
            std::to_string(npc.posZ) + ")");
    }

    req::shared::logInfo("zone", std::string{"[SPAWN] Instantiated "} + std::to_string(spawnedCount) +
        " NPC(s) from " + std::to_string(spawnTable_.spawnPoints.size()) + " spawn point(s)");
}
```

---

### Step 10: Handle Respawns with Spawn Groups

**File:** `REQ_ZoneServer/src/ZoneServer_Npc.cpp`  
**Function:** `void ZoneServer::updateNpc()`  
**Location:** In the respawn logic where NPC comes back to life

**Current code respawns the same NPC. Need to:**
1. Store `spawnId` in ZoneNpc struct
2. When respawning, re-roll spawn group if applicable
3. Potentially spawn a different NPC from the group

**Add to ZoneNpc struct in DataModels.h:**
```cpp
// Link to spawn system
std::int32_t spawnId{ 0 };           // Which spawn point created this NPC
std::int32_t templateId{ 0 };        // Which template was used
```

**Then in respawn logic, replace direct respawn with:**
```cpp
if (npc.respawnTimerSec <= 0.0f) {
    // Find the spawn point that created this NPC
    const req::shared::data::SpawnPoint* spawnPoint = nullptr;
    for (const auto& sp : spawnTable_.spawnPoints) {
        if (sp.spawnId == npc.spawnId) {
            spawnPoint = &sp;
            break;
        }
    }

    if (spawnPoint == nullptr) {
        // Fallback: just respawn same NPC
        npc.posX = npc.spawnX;
        npc.posY = npc.spawnY;
        npc.posZ = npc.spawnZ;
        npc.currentHp = npc.maxHp;
        npc.isAlive = true;
        npc.pendingRespawn = false;
        npc.state = AiState::Idle;
        // ... rest of reset
    } else {
        // Re-instantiate from spawn point (may be different NPC from group)
        // TODO: Implement re-roll logic similar to initial spawn
        // For now, just respawn same NPC
        npc.posX = npc.spawnX;
        npc.posY = npc.spawnY;
        npc.posZ = npc.spawnZ;
        npc.currentHp = npc.maxHp;
        npc.isAlive = true;
        npc.pendingRespawn = false;
        npc.state = AiState::Idle;
        // ... rest of reset
    }
}
```

---

## Testing Checklist

### Build
- [ ] Project compiles with no errors
- [ ] All new loaders compile
- [ ] ZoneServer constructor compiles with template loading
- [ ] instantiateNpcsFromSpawnTable compiles

### Runtime
- [ ] ZoneServer loads `data/npcs.json` successfully
- [ ] ZoneServer logs loaded template count
- [ ] ZoneServer loads `data/spawns_10.json` for zone 10
- [ ] ZoneServer logs spawn point and group counts
- [ ] NPCs are instantiated from spawn points
- [ ] NPCs have correct names from templates
- [ ] NPCs have correct stats from templates
- [ ] NPCs spawn at correct positions from spawn points
- [ ] Weighted spawn groups work (varied NPCs at same spawn point)
- [ ] Direct NPC IDs work (same NPC always spawns)

### Manual Test
```bash
cd x64/Debug
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7780 --zone_name="East Freeport"
```

**Expected logs:**
```
[INFO] [zone] Loaded 6 NPC template(s)
[INFO] [zone] Loaded spawn table: 6 spawn point(s), 3 spawn group(s)
[INFO] [SPAWN] Spawned NPC: id=1, template=1001, name="A Decaying Skeleton", level=1, spawnId=1, pos=(100,50,0)
[INFO] [SPAWN] Spawned NPC: id=2, template=1002, name="A Giant Rat", level=1, spawnId=2, pos=(200,75,0)
...
[INFO] [SPAWN] Instantiated 6 NPC(s) from 6 spawn point(s)
```

---

## Files Modified Summary

| File | Status | Changes |
|------|--------|---------|
| `REQ_Shared/include/req/shared/DataModels.h` | ? Complete | Added NPC template and spawn structures |
| `REQ_Shared/include/req/shared/Config.h` | ? Complete | Added loader declarations |
| `REQ_Shared/src/Config.cpp` | ? Complete | Implemented loaders |
| `data/npcs.json` | ? Complete | Created with 6 templates |
| `data/spawns_10.json` | ? Complete | Created with 6 spawn points |
| `data/spawns_20.json` | ? Complete | Created with 5 spawn points |
| `REQ_ZoneServer/include/req/zone/ZoneServer.h` | ?? Partial | Added members, need to add instantiate function decl |
| `REQ_ZoneServer/src/ZoneServer.cpp` | ?? Todo | Load templates in constructor, load spawn table in run() |
| `REQ_ZoneServer/src/ZoneServer_Npc.cpp` | ?? Todo | Implement instantiateNpcsFromSpawnTable() |

---

## Quick Implementation Guide

1. **Copy template loading code** into `ZoneServer::ZoneServer()` constructor
2. **Replace `loadNpcsForZone()`** call in `ZoneServer::run()` with spawn table loading
3. **Add `instantiateNpcsFromSpawnTable()`** declaration to ZoneServer.h private section
4. **Implement `instantiateNpcsFromSpawnTable()`** in ZoneServer_Npc.cpp
5. **Build and test**

---

## Benefits of New System

? **Data-driven NPC population** - No code changes to add/modify NPCs  
? **Reusable templates** - Define once, spawn many times  
? **Weighted spawn groups** - Common/uncommon/rare spawns  
? **Flexible spawn points** - Direct NPC or group selection  
? **Behavior configuration** - Aggro, leash, social, flee params  
? **Level ranges** - Randomized level within template bounds  
? **Respawn variance** - Configurable respawn timers per spawn point  
? **Day/night spawns** - Ready for future time-of-day system  
? **Named chance** - Ready for future named/boss spawns  

---

## Next Phase

Once this is complete, future enhancements can include:

- **Named/Boss spawns** - Use `namedChance` to replace with rare variant
- **Day/night cycles** - Enable `dayOnly`/`nightOnly` spawns
- **Roaming NPCs** - Use `roamRadius` for patrol behavior
- **Dynamic spawns** - Hot zones with increased density
- **Loot tables** - Use `lootTableId` to generate drops
- **Faction system** - Use `factionId` for friend/foe logic
- **Ability packages** - Load spell/ability sets per template
- **Visual packages** - Client-side model/animation selection

---

**Status:** 75% Complete  
**Blockers:** None  
**Next:** Implement remaining ZoneServer integration steps
