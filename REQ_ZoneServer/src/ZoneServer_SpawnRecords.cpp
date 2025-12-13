#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <random>
#include <chrono>

// Using declarations for NPC spawn data types
using req::zone::NpcTemplateData;
using req::zone::NpcSpawnPointData;

namespace req::zone {

// ============================================================================
// Spawn Manager - Lifecycle Management
// ============================================================================

void ZoneServer::initializeSpawnRecords() {
    req::shared::logInfo("zone", "[SPAWN] === Initializing Spawn Records ===");
    
    const auto& spawns = npcDataRepository_.GetZoneSpawns();
    if (spawns.empty()) {
        req::shared::logInfo("zone", "[SPAWN] No spawn points defined for this zone");
        return;
    }
    
    // Get current time for initial spawn scheduling
    auto now = std::chrono::system_clock::now();
    double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
    
    int recordCount = 0;
    for (const auto& spawn : spawns) {
        // Verify NPC template exists
        const NpcTemplateData* tmpl = npcDataRepository_.GetTemplate(spawn.npcId);
        if (!tmpl) {
            req::shared::logWarn("zone", std::string{"[SPAWN] Spawn point "} +
                std::to_string(spawn.spawnId) + " references unknown NPC template: " +
                std::to_string(spawn.npcId) + ", skipping");
            continue;
        }
        
        // Create spawn record
        SpawnRecord record;
        record.spawn_point_id = spawn.spawnId;
        record.npc_template_id = spawn.npcId;
        record.posX = spawn.posX;
        record.posY = spawn.posY;
        record.posZ = spawn.posZ;
        record.heading = spawn.heading;
        record.respawn_seconds = static_cast<float>(spawn.respawnSeconds);
        record.respawn_jitter_seconds = static_cast<float>(spawn.respawnVarianceSeconds);
        
        // IMMEDIATE INITIAL SPAWN: Schedule for immediate spawn (currentTime + tiny epsilon)
        // This ensures NPCs appear instantly on zone start, no confusing 0-10 second delay
        record.state = SpawnState::WaitingToSpawn;
        record.next_spawn_time = currentTime + 0.1;  // Tiny delay for deterministic ordering
        record.current_entity_id = 0;
        
        // Store in map
        spawnRecords_[spawn.spawnId] = record;
        recordCount++;
        
        if (enableSpawnDebugLogging_) {
            req::shared::logInfo("zone", std::string{"[SPAWN] Initialized spawn record: spawn_id="} +
                std::to_string(spawn.spawnId) + ", npc_id=" + std::to_string(spawn.npcId) +
                " (" + tmpl->name + "), initial_spawn=immediate");
        }
    }
    
    req::shared::logInfo("zone", std::string{"[SPAWN] Initial spawns scheduled immediate (0.1s), "} +
        std::to_string(recordCount) + " spawn record(s) initialized");
}

void ZoneServer::processSpawns(float deltaSeconds, double currentTime) {
    for (auto& [spawnId, record] : spawnRecords_) {
        if (record.state == SpawnState::WaitingToSpawn) {
            // Check if it's time to spawn
            if (currentTime >= record.next_spawn_time) {
                spawnNpcAtPoint(record, currentTime);
            }
        }
        // Alive spawns are managed by NPC death system
    }
}

void ZoneServer::spawnNpcAtPoint(SpawnRecord& record, double currentTime) {
    // Get NPC template
    const NpcTemplateData* tmpl = npcDataRepository_.GetTemplate(record.npc_template_id);
    if (!tmpl) {
        req::shared::logError("zone", std::string{"[SPAWN] Cannot spawn - template not found: npc_id="} +
            std::to_string(record.npc_template_id) + ", spawn_id=" + std::to_string(record.spawn_point_id));
        
        // Reschedule spawn to retry later
        record.next_spawn_time = currentTime + record.respawn_seconds;
        return;
    }
    
    // Create runtime NPC instance from template
    req::shared::data::ZoneNpc npc;
    
    // Generate unique instance ID
    npc.npcId = nextNpcInstanceId_++;
    
    // Copy data from template
    npc.name = tmpl->name;
    npc.level = tmpl->level;
    npc.templateId = tmpl->npcId;
    npc.spawnId = record.spawn_point_id;
    npc.factionId = tmpl->factionId;
    
    // Set stats from template
    npc.maxHp = tmpl->hp;
    npc.currentHp = npc.maxHp;
    npc.isAlive = true;
    npc.minDamage = tmpl->minDamage;
    npc.maxDamage = tmpl->maxDamage;
    
    // Position from spawn point
    npc.posX = record.posX;
    npc.posY = record.posY;
    npc.posZ = record.posZ;
    npc.facingDegrees = record.heading;
    
    // Store spawn point for respawn/leashing
    npc.spawnX = record.posX;
    npc.spawnY = record.posY;
    npc.spawnZ = record.posZ;
    
    // Respawn timing from spawn point
    npc.respawnTimeSec = record.respawn_seconds;
    npc.respawnTimerSec = 0.0f;
    npc.pendingRespawn = false;
    
    // Behavior from template
    npc.behaviorFlags.isSocial = tmpl->isSocial;
    npc.behaviorFlags.canFlee = tmpl->canFlee;
    npc.behaviorFlags.isRoamer = tmpl->isRoamer;
    npc.behaviorFlags.leashToSpawn = true;
    
    npc.behaviorParams.aggroRadius = tmpl->aggroRadius;
    npc.behaviorParams.socialRadius = tmpl->assistRadius;
    npc.behaviorParams.leashRadius = 2000.0f;  // Default leash radius
    npc.behaviorParams.maxChaseDistance = 2500.0f;  // Default max chase
    npc.behaviorParams.preferredRange = 200.0f;  // Melee range
    npc.behaviorParams.fleeHealthPercent = tmpl->canFlee ? 0.25f : 0.0f;
    
    // AI state
    npc.aiState = req::shared::data::NpcAiState::Idle;
    npc.currentTargetId = 0;
    npc.hateTable.clear();
    
    // Attack timing
    npc.meleeAttackCooldown = 1.5f;  // Default attack speed
    npc.meleeAttackTimer = 0.0f;
    npc.aggroScanTimer = 0.0f;
    npc.leashTimer = 0.0f;
    
    // Movement
    npc.moveSpeed = 50.0f;  // Default movement speed
    
    // Add to zone
    npcs_[npc.npcId] = npc;
    
    // DIAGNOSTIC: Log spawn origin for duplicate tracking
    req::shared::logInfo("zone", std::string{"[SPAWN_ORIGIN] tag=SpawnNpcAtPoint npcId="} +
        std::to_string(npc.npcId) + " spawnPointId=" + std::to_string(record.spawn_point_id) +
        " templateId=" + std::to_string(npc.templateId) +
        " pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," + std::to_string(npc.posZ) + ")" +
        " isAlive=" + (npc.isAlive ? "true" : "false"));
    
    // Update spawn record
    record.state = SpawnState::Alive;
    record.current_entity_id = npc.npcId;
    
    req::shared::logInfo("zone", std::string{"[SPAWN] Spawned NPC: instanceId="} +
        std::to_string(npc.npcId) + ", templateId=" + std::to_string(npc.templateId) +
        ", name=\"" + npc.name + "\", level=" + std::to_string(npc.level) +
        ", spawnId=" + std::to_string(record.spawn_point_id) +
        ", pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," +
        std::to_string(npc.posZ) + ")" +
        ", hp=" + std::to_string(npc.currentHp) + "/" + std::to_string(npc.maxHp));
    
    // CRITICAL: Broadcast EntitySpawn to all clients
    broadcastEntitySpawn(npc.npcId);
}

void ZoneServer::scheduleRespawn(std::int32_t spawnPointId, double currentTime) {
    auto it = spawnRecords_.find(spawnPointId);
    if (it == spawnRecords_.end()) {
        req::shared::logWarn("zone", std::string{"[SPAWN] Cannot schedule respawn - spawn point not found: spawn_id="} +
            std::to_string(spawnPointId));
        return;
    }
    
    SpawnRecord& record = it->second;
    
    // DIAGNOSTIC: Log previous state before scheduling respawn
    std::string prevStateStr = (record.state == SpawnState::Alive) ? "Alive" : "WaitingToSpawn";
    std::uint64_t prevEntityId = record.current_entity_id;
    
    // Calculate next spawn time with jitter
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitterDist(-record.respawn_jitter_seconds, record.respawn_jitter_seconds);
    float jitter = jitterDist(gen);
    
    record.state = SpawnState::WaitingToSpawn;
    record.next_spawn_time = currentTime + record.respawn_seconds + jitter;
    record.current_entity_id = 0;
    
    // DIAGNOSTIC: Log respawn schedule event
    req::shared::logInfo("zone", std::string{"[RESPAWN_SCHEDULE] spawnPointId="} +
        std::to_string(spawnPointId) + " prevState=" + prevStateStr +
        " nextSpawnTime=" + std::to_string(record.next_spawn_time) +
        " currentEntityId=" + std::to_string(prevEntityId) +
        " respawnDelay=" + std::to_string(record.respawn_seconds + jitter) + "s");
    
    req::shared::logInfo("zone", std::string{"[SPAWN] Scheduled respawn: spawn_id="} +
        std::to_string(spawnPointId) + ", npc_id=" + std::to_string(record.npc_template_id) +
        ", respawn_in=" + std::to_string(record.respawn_seconds + jitter) + "s");
}

} // namespace req::zone
