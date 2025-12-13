#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <fstream>
#include <random>
#include <cmath>
#include <algorithm>

// Using declarations for NPC spawn data types
using req::zone::NpcTemplateData;
using req::zone::NpcSpawnPointData;

namespace {
    constexpr float MAX_HATE = 1.0e9f; // 1 billion - prevents unbounded growth
}

namespace req::zone {

// ============================================================================
// Hate/Aggro System (Phase 2.3)
// ============================================================================

void ZoneServer::addHate(req::shared::data::ZoneNpc& npc, std::uint64_t entityId, float amount) {
if (entityId == 0 || amount <= 0.0f) {
    return;
}

// Add or increment hate for this entity with cap
auto& value = npc.hateTable[entityId];
value = std::min(value + amount, MAX_HATE);

    // Update current target if needed
    std::uint64_t previousTarget = npc.currentTargetId;
    std::uint64_t newTopTarget = getTopHateTarget(npc);

    if (newTopTarget != previousTarget) {
        npc.currentTargetId = newTopTarget;

        // Log target swap
        float topHate = (newTopTarget != 0) ? npc.hateTable[newTopTarget] : 0.0f;
        req::shared::logInfo("zone", std::string{"[HATE] NPC "} + std::to_string(npc.npcId) +
            " \"" + npc.name + "\" new_target=" + std::to_string(newTopTarget) +
            " top_hate=" + std::to_string(topHate));
    }
}

std::uint64_t ZoneServer::getTopHateTarget(const req::shared::data::ZoneNpc& npc) const {
    if (npc.hateTable.empty()) {
        return 0;
    }

    std::uint64_t topTarget = 0;
    float maxHate = 0.0f;

    for (const auto& [entityId, hate] : npc.hateTable) {
        if (hate > maxHate) {
            maxHate = hate;
            topTarget = entityId;
        }
    }

    return topTarget;
}

void ZoneServer::clearHate(req::shared::data::ZoneNpc& npc) {
    npc.hateTable.clear();
    npc.currentTargetId = 0;

    req::shared::logInfo("zone", std::string{"[HATE] Cleared hate for NPC "} +
        std::to_string(npc.npcId) + " \"" + npc.name + "\"");
}

void ZoneServer::removeCharacterFromAllHateTables(std::uint64_t characterId) {
    int numNpcsTouched = 0;
    int numTablesCleared = 0;

    for (auto& [npcId, npc] : npcs_) {
        // Check if this NPC has hate for the character
        auto hateIt = npc.hateTable.find(characterId);
        if (hateIt == npc.hateTable.end()) {
            continue; // NPC doesn't have hate for this character
        }

        // Remove hate entry
        npc.hateTable.erase(hateIt);
        numNpcsTouched++;

        // Check if hate table is now empty
        if (npc.hateTable.empty()) {
            numTablesCleared++;
        }

        // If this was the current target, recompute target
        if (npc.currentTargetId == characterId) {
            std::uint64_t newTarget = getTopHateTarget(npc);
            npc.currentTargetId = newTarget;

            // If no new target and NPC is engaged, transition to leashing
            if (newTarget == 0 && npc.aiState == req::shared::data::NpcAiState::Engaged) {
                npc.aiState = req::shared::data::NpcAiState::Leashing;

                req::shared::logInfo("zone", std::string{"[HATE] NPC "} + std::to_string(npc.npcId) +
                    " \"" + npc.name + "\" lost target (character removed), transitioning to Leashing");
            }
        }
    }

    if (numNpcsTouched > 0) {
        req::shared::logInfo("zone", std::string{"[HATE] Removed characterId="} +
            std::to_string(characterId) + " from " + std::to_string(numNpcsTouched) +
            " NPC hate table(s) (" + std::to_string(numTablesCleared) + " cleared)");
    }
}

// ============================================================================
// NPC AI State Machine (Phase 2.3)
// ============================================================================

void ZoneServer::updateNpcAi(req::shared::data::ZoneNpc& npc, float dt) {
    using NpcAiState = req::shared::data::NpcAiState;

    // Dead NPCs are handled by respawn system
    if (!npc.isAlive) {
        if (npc.aiState != NpcAiState::Dead) {
            npc.aiState = NpcAiState::Dead;
        }
        return;
    }

    // Update AI timers
    npc.aggroScanTimer -= dt;
    if (npc.aggroScanTimer < 0.0f) {
        npc.aggroScanTimer = 0.0f;
    }

    if (npc.meleeAttackTimer > 0.0f) {
        npc.meleeAttackTimer -= dt;
    }

    // AI State Machine
    switch (npc.aiState) {
        case NpcAiState::Idle: {
            // Low-frequency proximity scan (every 0.5-1.0s)
            if (npc.aggroScanTimer <= 0.0f) {
                npc.aggroScanTimer = 0.5f + (static_cast<float>(rand()) / RAND_MAX) * 0.5f;  // 0.5-1.0s

                // Scan for players within aggro radius
                const float aggroRadiusUnits = npc.behaviorParams.aggroRadius;

                for (const auto& [characterId, player] : players_) {
                    if (!player.isInitialized || player.isDead) {
                        continue;
                    }

                    // Calculate distance
                    float dx = player.posX - npc.posX;
                    float dy = player.posY - npc.posY;
                    float dz = player.posZ - npc.posZ;
                    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

                    // Check if player is within aggro range
                    if (distance <= aggroRadiusUnits) {
                        // Proximity aggro!
                        addHate(npc, characterId, 1.0f);  // Initial hate
                        npc.aiState = NpcAiState::Alert;

                        req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                            " \"" + npc.name + "\" state=Idle->Alert (proximity aggro)" +
                            ", target=" + std::to_string(characterId) +
                            ", distance=" + std::to_string(distance));
                        break;
                    }
                }
            }
            break;
        }

        case NpcAiState::Alert: {
            // Quick validation before engaging
            if (npc.currentTargetId == 0) {
                // No target, return to idle
                clearHate(npc);
                npc.aiState = NpcAiState::Idle;

                req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                    " state=Alert->Idle (no target)");
                break;
            }

            // Check if target still exists and is alive
            auto targetIt = players_.find(npc.currentTargetId);
            if (targetIt == players_.end() || !targetIt->second.isInitialized || targetIt->second.isDead) {
                // Target invalid, return to idle
                clearHate(npc);
                npc.aiState = NpcAiState::Idle;

                req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                    " state=Alert->Idle (invalid target)");
                break;
            }

            // Target valid, engage!
            npc.aiState = NpcAiState::Engaged;

            req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                " \"" + npc.name + "\" state=Alert->Engaged" +
                ", target=" + std::to_string(npc.currentTargetId));

            // Social aggro: alert nearby NPCs
            if (npc.behaviorFlags.isSocial) {
                const float socialRadiusUnits = npc.behaviorParams.socialRadius;

                for (auto& [otherId, otherNpc] : npcs_) {
                    if (otherId == npc.npcId || !otherNpc.isAlive) {
                        continue;
                    }

                    // Check same faction (simple check for now)
                    if (otherNpc.factionId != npc.factionId) {
                        continue;
                    }

                    // Check distance
                    float dx = otherNpc.posX - npc.posX;
                    float dy = otherNpc.posY - npc.posY;
                    float dz = otherNpc.posZ - npc.posZ;
                    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

                    if (distance <= socialRadiusUnits) {
                        // Alert this NPC
                        if (otherNpc.aiState == NpcAiState::Idle) {
                            addHate(otherNpc, npc.currentTargetId, 0.5f);  // Social hate
                            otherNpc.aiState = NpcAiState::Alert;

                            req::shared::logInfo("zone", std::string{"[AI] Social assist: NPC "} +
                                std::to_string(otherId) + " \"" + otherNpc.name + "\"" +
                                " assisting NPC " + std::to_string(npc.npcId) +
                                ", distance=" + std::to_string(distance));
                        }
                    }
                }
            }
            break;
        }

        case NpcAiState::Engaged: {
            // Get current target
            std::uint64_t targetId = getTopHateTarget(npc);
            if (targetId == 0) {
                // No target, leash back
                npc.aiState = NpcAiState::Leashing;

                req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                    " state=Engaged->Leashing (no target)");
                break;
            }

            auto targetIt = players_.find(targetId);
            if (targetIt == players_.end() || !targetIt->second.isInitialized || targetIt->second.isDead) {
                // Target died or disconnected, leash back
                clearHate(npc);
                npc.aiState = NpcAiState::Leashing;

                req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                    " state=Engaged->Leashing (target lost)");
                break;
            }

            ZonePlayer& target = targetIt->second;

            // Calculate distance to target
            float dx = target.posX - npc.posX;
            float dy = target.posY - npc.posY;
            float dz = target.posZ - npc.posZ;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            // Check leash conditions
            float dxSpawn = npc.posX - npc.spawnX;
            float dySpawn = npc.posY - npc.spawnY;
            float dzSpawn = npc.posZ - npc.spawnZ;
            float distFromSpawn = std::sqrt(dxSpawn * dxSpawn + dySpawn * dySpawn + dzSpawn * dzSpawn);

            const float leashRadiusUnits = npc.behaviorParams.leashRadius;
            const float maxChaseUnits = npc.behaviorParams.maxChaseDistance;

            if (npc.behaviorFlags.leashToSpawn && (distFromSpawn > leashRadiusUnits || distance > maxChaseUnits)) {
                clearHate(npc);
                npc.aiState = NpcAiState::Leashing;

                req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                    " state=Engaged->Leashing (exceeded leash)" +
                    ", distFromSpawn=" + std::to_string(distFromSpawn) +
                    ", distToTarget=" + std::to_string(distance));
                break;
            }

            // Check flee condition
            if (npc.behaviorFlags.canFlee && npc.behaviorParams.fleeHealthPercent > 0.0f) {
                float healthPercent = static_cast<float>(npc.currentHp) / static_cast<float>(npc.maxHp);
                if (healthPercent <= npc.behaviorParams.fleeHealthPercent) {
                    npc.aiState = NpcAiState::Fleeing;

                    req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                        " \"" + npc.name + "\" state=Engaged->Fleeing" +
                        ", hp=" + std::to_string(npc.currentHp) + "/" + std::to_string(npc.maxHp));
                    break;
                }
            }

            // Move toward target and attack
            const float meleeRange = npc.behaviorParams.preferredRange;

            if (distance > meleeRange) {
                // Move toward target
                float moveX = dx / distance;
                float moveY = dy / distance;

                npc.posX += moveX * npc.moveSpeed * dt;
                npc.posY += moveY * npc.moveSpeed * dt;

                // Update facing
                npc.facingDegrees = std::atan2(dy, dx) * 180.0f / 3.14159f;
            } else {
                // In melee range - attack if cooldown ready
                if (npc.meleeAttackTimer <= 0.0f) {
                    // Perform melee attack
                    static std::mt19937 rng(std::random_device{}());
                    std::uniform_int_distribution<int> damageDist(npc.minDamage, npc.maxDamage);
                    int damage = damageDist(rng);

                    // Apply damage
                    target.hp -= damage;
                    target.combatStatsDirty = true;

                    req::shared::logInfo("zone", std::string{"[COMBAT] NPC "} + std::to_string(npc.npcId) +
                        " \"" + npc.name + "\" melee attack" +
                        ", target=" + std::to_string(target.characterId) +
                        ", damage=" + std::to_string(damage) +
                        ", targetHp=" + std::to_string(target.hp) + "/" + std::to_string(target.maxHp));

                    // Check if player died
                    if (target.hp <= 0) {
                        handlePlayerDeath(target);
                        clearHate(npc);
                        npc.aiState = NpcAiState::Leashing;

                        req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                            " state=Engaged->Leashing (target died)");
                    }

                    // Reset attack cooldown
                    npc.meleeAttackTimer = npc.meleeAttackCooldown;
                }
            }
            break;
        }

        case NpcAiState::Leashing: {
            // Move back to spawn point
            float dx = npc.spawnX - npc.posX;
            float dy = npc.spawnY - npc.posY;
            float dz = npc.spawnZ - npc.posZ;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            constexpr float SPAWN_EPSILON = 2.0f;

            if (distance <= SPAWN_EPSILON) {
                // Reached spawn - reset to idle
                npc.posX = npc.spawnX;
                npc.posY = npc.spawnY;
                npc.posZ = npc.spawnZ;
                npc.currentHp = npc.maxHp;  // Heal to full on leash
                clearHate(npc);
                npc.aiState = NpcAiState::Idle;

                req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                    " state=Leashing->Idle (reached spawn, reset)");
            } else {
                // Move toward spawn
                float moveX = dx / distance;
                float moveY = dy / distance;

                npc.posX += moveX * npc.moveSpeed * dt;
                npc.posY += moveY * npc.moveSpeed * dt;
            }
            break;
        }

        case NpcAiState::Fleeing: {
            // Move away from current target
            if (npc.currentTargetId != 0) {
                auto targetIt = players_.find(npc.currentTargetId);
                if (targetIt != players_.end() && targetIt->second.isInitialized && !targetIt->second.isDead) {
                    const ZonePlayer& target = targetIt->second;

                    // Move away from target
                    float dx = npc.posX - target.posX;
                    float dy = npc.posY - target.posY;
                    float distance = std::sqrt(dx * dx + dy * dy);

                    if (distance > 0.01f) {
                        float moveX = dx / distance;
                        float moveY = dy / distance;

                        npc.posX += moveX * npc.moveSpeed * dt;
                        npc.posY += moveY * npc.moveSpeed * dt;

                        // Update facing (running away)
                        npc.facingDegrees = std::atan2(moveY, moveX) * 180.0f / 3.14159f;
                    }
                }
            }

            // Check if far enough to switch to leashing
            float dxSpawn = npc.posX - npc.spawnX;
            float dySpawn = npc.posY - npc.spawnY;
            float distFromSpawn = std::sqrt(dxSpawn * dxSpawn + dySpawn * dySpawn);

            const float leashRadiusUnits = npc.behaviorParams.leashRadius;
            if (distFromSpawn > leashRadiusUnits * 0.8f) {  // 80% of leash radius
                npc.aiState = NpcAiState::Leashing;

                req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(npc.npcId) +
                    " state=Fleeing->Leashing (reached safe distance)");
            }
            break;
        }

        case NpcAiState::Dead: {
            // Already handled at top of function
            break;
        }
    }
}

// ============================================================================
// Original functions (kept for compatibility)
// ============================================================================

void ZoneServer::loadNpcsForZone() {
    // Deprecated: This function loads NPCs from old zone_X_npcs.json format
    // New system uses NPC templates (npcs.json) + spawn tables (spawns_X.json)
    // Kept temporarily for backward compatibility

    req::shared::logWarn("zone", "[NPC] loadNpcsForZone() is deprecated - use spawn table system instead");

    // NOTE: This old loader is not compatible with new ZoneNpc structure
    // To use old NPC files, they need to be converted to the new template + spawn format
    // See config/PHASE2_SPAWN_SYSTEM_STATUS.md for migration guide
}

void ZoneServer::updateNpc(req::shared::data::ZoneNpc& npc, float deltaSeconds) {
    using NpcAiState = req::shared::data::NpcAiState;

    // Handle dead/respawning NPCs
    if (!npc.isAlive) {
        if (!npc.pendingRespawn) {
            // Start respawn timer
            npc.pendingRespawn = true;
            npc.respawnTimerSec = npc.respawnTimeSec;
            npc.aiState = NpcAiState::Dead;

            req::shared::logInfo("zone", std::string{"[NPC] NPC died, respawn in "} +
                std::to_string(npc.respawnTimeSec) + "s: id=" + std::to_string(npc.npcId) +
                ", name=\"" + npc.name + "\"");
        } else {
            // Count down respawn timer
            npc.respawnTimerSec -= deltaSeconds;

            if (npc.respawnTimerSec <= 0.0f) {
                // Respawn NPC
                npc.posX = npc.spawnX;
                npc.posY = npc.spawnY;
                npc.posZ = npc.spawnZ;
                npc.currentHp = npc.maxHp;
                npc.isAlive = true;
                npc.pendingRespawn = false;
                npc.respawnTimerSec = 0.0f;
                npc.aiState = NpcAiState::Idle;
                clearHate(npc);
                npc.meleeAttackTimer = 0.0f;

                req::shared::logInfo("zone", std::string{"[NPC] Respawned: id="} +
                    std::to_string(npc.npcId) + ", name=\"" + npc.name + "\"" +
                    ", pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," +
                    std::to_string(npc.posZ) + ")");
            }
        }
        return;
    }

    // Run AI state machine
    updateNpcAi(npc, deltaSeconds);
}

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
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
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
        
        // Initialize as WaitingToSpawn with small random offset (0-10 seconds)
        record.state = SpawnState::WaitingToSpawn;
        std::uniform_real_distribution<float> offsetDist(0.0f, 10.0f);
        float initialOffset = offsetDist(gen);
        record.next_spawn_time = currentTime + initialOffset;
        record.current_entity_id = 0;
        
        // Store in map
        spawnRecords_[spawn.spawnId] = record;
        recordCount++;
        
        if (enableSpawnDebugLogging_) {
            req::shared::logInfo("zone", std::string{"[SPAWN] Initialized spawn record: spawn_id="} +
                std::to_string(spawn.spawnId) + ", npc_id=" + std::to_string(spawn.npcId) +
                " (" + tmpl->name + ")" +
                ", initial_spawn_in=" + std::to_string(initialOffset) + "s");
        }
    }
    
    req::shared::logInfo("zone", std::string{"[SPAWN] Initialized "} + std::to_string(recordCount) +
        " spawn record(s) from " + std::to_string(spawns.size()) + " spawn point(s)");
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
    
    // Update spawn record
    record.state = SpawnState::Alive;
    record.current_entity_id = npc.npcId;
    
    req::shared::logInfo("zone", std::string{"[SPAWN] Spawned NPC: instanceId="} +
        std::to_string(npc.npcId) + ", templateId=" + std::to_string(npc.templateId) +
        ", name=\"" + npc.name + "\", level=" + std::to_string(npc.level) +
        ", spawnId=" + std::to_string(record.spawn_point_id) +
        ", pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," +
        std::to_string(npc.posZ) + ")");
}

void ZoneServer::scheduleRespawn(std::int32_t spawnPointId, double currentTime) {
    auto it = spawnRecords_.find(spawnPointId);
    if (it == spawnRecords_.end()) {
        req::shared::logWarn("zone", std::string{"[SPAWN] Cannot schedule respawn - spawn point not found: spawn_id="} +
            std::to_string(spawnPointId));
        return;
    }
    
    SpawnRecord& record = it->second;
    
    // Calculate next spawn time with jitter
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitterDist(-record.respawn_jitter_seconds, record.respawn_jitter_seconds);
    float jitter = jitterDist(gen);
    
    record.state = SpawnState::WaitingToSpawn;
    record.next_spawn_time = currentTime + record.respawn_seconds + jitter;
    record.current_entity_id = 0;
    
    req::shared::logInfo("zone", std::string{"[SPAWN] Scheduled respawn: spawn_id="} +
        std::to_string(spawnPointId) + ", npc_id=" + std::to_string(record.npc_template_id) +
        ", respawn_in=" + std::to_string(record.respawn_seconds + jitter) + "s");
}

// ============================================================================
// Debug / Inspection Tools
// ============================================================================

void ZoneServer::debugNpcHate(std::uint64_t npcId) {
    auto npcIt = npcs_.find(npcId);
    if (npcIt == npcs_.end()) {
        req::shared::logWarn("zone", std::string{"[HATE] debug_hate failed - NPC not found: npcId="} +
            std::to_string(npcId));
        return;
    }
    
    const auto& npc = npcIt->second;
    
    // Convert AI state to string for logging
    std::string stateStr;
    switch (npc.aiState) {
        case req::shared::data::NpcAiState::Idle: stateStr = "Idle"; break;
        case req::shared::data::NpcAiState::Alert: stateStr = "Alert"; break;
        case req::shared::data::NpcAiState::Engaged: stateStr = "Engaged"; break;
        case req::shared::data::NpcAiState::Leashing: stateStr = "Leashing"; break;
        case req::shared::data::NpcAiState::Fleeing: stateStr = "Fleeing"; break;
        case req::shared::data::NpcAiState::Dead: stateStr = "Dead"; break;
        default: stateStr = "Unknown"; break;
    }
    
    req::shared::logInfo("zone", std::string{"[HATE] NPC "} + std::to_string(npcId) +
        " (name='" + npc.name + "', state=" + stateStr +
        ", currentTargetId=" + std::to_string(npc.currentTargetId) + ") hate table:");
    
    if (npc.hateTable.empty()) {
        req::shared::logInfo("zone", "[HATE]   (no hate entries)");
    } else {
        for (const auto& [entityId, hate] : npc.hateTable) {
            // Check if entity is a valid player and alive
            const auto it = players_.find(entityId);
            bool alive = false;
            std::string targetInfo = "unknown";
            
            if (it != players_.end()) {
                alive = !it->second.isDead && it->second.isInitialized;
                targetInfo = std::string("player") + (alive ? " (alive)" : " (dead)");
            }
            
            req::shared::logInfo("zone", std::string{"[HATE]   target="} + std::to_string(entityId) +
                " hate=" + std::to_string(hate) + " [" + targetInfo + "]");
        }
    }
}

} // namespace req::zone
