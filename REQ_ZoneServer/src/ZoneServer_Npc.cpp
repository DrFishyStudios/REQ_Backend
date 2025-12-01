#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <fstream>
#include <random>
#include <cmath>
#include <algorithm>

namespace req::zone {

// ============================================================================
// Hate/Aggro System (Phase 2.3)
// ============================================================================

void ZoneServer::addHate(req::shared::data::ZoneNpc& npc, std::uint64_t entityId, float amount) {
    if (entityId == 0 || amount <= 0.0f) {
        return;
    }

    // Add or increment hate for this entity
    npc.hateTable[entityId] += amount;

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
                const float aggroRadiusUnits = npc.behaviorParams.aggroRadius / 10.0f;  // Convert from GDD units

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
                const float socialRadiusUnits = npc.behaviorParams.socialRadius / 10.0f;

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

            const float leashRadiusUnits = npc.behaviorParams.leashRadius / 10.0f;
            const float maxChaseUnits = npc.behaviorParams.maxChaseDistance / 10.0f;

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
            const float meleeRange = npc.behaviorParams.preferredRange / 10.0f;

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

            const float leashRadiusUnits = npc.behaviorParams.leashRadius / 10.0f;
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

} // namespace req::zone
