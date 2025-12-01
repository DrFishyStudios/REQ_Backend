#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <cmath>
#include <algorithm>
#include <chrono>

namespace req::zone {

namespace {
    // Simulation constants
    constexpr float TICK_RATE_HZ = 20.0f;                    // 20 ticks per second
    constexpr auto TICK_INTERVAL_MS = std::chrono::milliseconds(50);  // 50ms per tick
    constexpr float TICK_DT = 1.0f / TICK_RATE_HZ;           // 0.05 seconds
    
    // TODO: configurable movement speed (now using zoneConfig_.moveSpeed instead)
    // Old default was 7.0f - now defaults to 70.0f for visible movement
    
    constexpr float GRAVITY = -30.0f;                        // units per second squared
    constexpr float JUMP_VELOCITY = 10.0f;                   // initial jump velocity
    constexpr float GROUND_LEVEL = 0.0f;                     // Z coordinate of ground
    
    constexpr float MAX_ALLOWED_MOVE_MULTIPLIER = 1.5f;     // slack for network jitter
    constexpr float SUSPICIOUS_MOVE_MULTIPLIER = 5.0f;      // clearly insane movement
}

void ZoneServer::scheduleNextTick() {
    tickTimer_.expires_after(TICK_INTERVAL_MS);
    tickTimer_.async_wait([this](const boost::system::error_code& ec) {
        onTick(ec);
    });
}

void ZoneServer::onTick(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
        req::shared::logInfo("zone", "Tick timer cancelled (server shutting down)");
        return;
    }
    
    if (ec) {
        req::shared::logError("zone", std::string{"Tick timer error: "} + ec.message());
        return;
    }
    
    // Update simulation with fixed timestep
    updateSimulation(TICK_DT);
    
    // Broadcast state snapshots to all clients
    broadcastSnapshots();
    
    // Schedule next tick
    scheduleNextTick();
}

void ZoneServer::updateSimulation(float dt) {
    // Log simulation update periodically (every 20 ticks = 1 second at 20Hz)
    static std::uint64_t simLogCounter = 0;
    bool doDetailedLog = (++simLogCounter % 20 == 0);
    
    // Get configurable move speed from zone config
    const float moveSpeed = zoneConfig_.moveSpeed;
    
    // Update player physics
    for (auto& [characterId, player] : players_) {
        if (!player.isInitialized) {
            continue;
        }
        
        // Skip physics updates for dead players
        if (player.isDead) {
            continue;
        }
        
        // Log initial state for debugging
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) +
                " BEFORE: pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")" +
                ", input=(" + std::to_string(player.inputX) + "," + std::to_string(player.inputY) + ")");
        }
        
        // Compute 2D movement direction from input
        float inputLength = std::sqrt(player.inputX * player.inputX + player.inputY * player.inputY);
        float dirX = player.inputX;
        float dirY = player.inputY;
        
        // Normalize diagonal movement
        if (inputLength > 1.0f) {
            dirX /= inputLength;
            dirY /= inputLength;
            inputLength = 1.0f;
        }
        
        // Compute desired velocity using configurable move speed
        player.velX = dirX * moveSpeed;
        player.velY = dirY * moveSpeed;
        
        // Compute expected move distance this frame
        const float maxMoveDist = moveSpeed * dt;
        
        // Log velocity computation with move details
        if (doDetailedLog && (std::abs(player.velX) > 0.01f || std::abs(player.velY) > 0.01f)) {
            req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) +
                " MOVE: pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")" +
                ", input=(" + std::to_string(dirX) + "," + std::to_string(dirY) + ")" +
                ", moveSpeed=" + std::to_string(moveSpeed) +
                ", dt=" + std::to_string(dt) +
                ", moveDist=" + std::to_string(maxMoveDist));
        }
        
        // Handle vertical movement / gravity
        bool isOnGround = (player.posZ <= GROUND_LEVEL);
        
        if (isOnGround) {
            // On ground: can jump
            if (player.isJumpPressed) {
                player.velZ = JUMP_VELOCITY;
                req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) + " jumped");
            } else {
                // Stay on ground
                player.velZ = 0.0f;
            }
        } else {
            // In air: apply gravity
            player.velZ += GRAVITY * dt;
        }
        
        // Integrate position
        float newPosX = player.posX + player.velX * dt;
        float newPosY = player.posY + player.velY * dt;
        float newPosZ = player.posZ + player.velZ * dt;
        
        // Clamp to ground level
        if (newPosZ <= GROUND_LEVEL) {
            newPosZ = GROUND_LEVEL;
            player.velZ = 0.0f;
        }
        
        // Basic anti-cheat / sanity check
        float dx = newPosX - player.lastValidPosX;
        float dy = newPosY - player.lastValidPosY;
        float dz = newPosZ - player.lastValidPosZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        
        float maxAllowedMove = moveSpeed * dt * MAX_ALLOWED_MOVE_MULTIPLIER;
        float suspiciousThreshold = maxAllowedMove * SUSPICIOUS_MOVE_MULTIPLIER;
        
        if (dist > suspiciousThreshold) {
            // Clearly insane movement - snap back
            req::shared::logWarn("zone", std::string{"Movement suspicious for characterId="} + 
                std::to_string(characterId) + ", dist=" + std::to_string(dist) + 
                " (max allowed=" + std::to_string(maxAllowedMove) + "), snapping back to last valid position");
            
            player.posX = player.lastValidPosX;
            player.posY = player.lastValidPosY;
            player.posZ = player.lastValidPosZ;
            player.velX = 0.0f;
            player.velY = 0.0f;
            player.velZ = 0.0f;
        } else {
            // Accept new position
            player.posX = newPosX;
            player.posY = newPosY;
            player.posZ = newPosZ;
            
            // Update last valid position
            player.lastValidPosX = newPosX;
            player.lastValidPosY = newPosY;
            player.lastValidPosZ = newPosZ;
            
            // Mark as dirty if position changed
            if (dist > 0.01f) {  // Small threshold to avoid marking on tiny movements
                player.isDirty = true;
            }
            
            // Log position change
            if (doDetailedLog) {
                req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) +
                    " AFTER: pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")" +
                    ", moved=" + std::to_string(dist) + " units");
            }
        }
    }
    
    // Update NPCs (AI state machine)
    for (auto& [npcId, npc] : npcs_) {
        updateNpc(npc, dt);
    }
    
    // Periodic NPC debug logging (every 5 seconds at 20Hz = 100 ticks)
    static std::uint64_t npcLogCounter = 0;
    if (!npcs_.empty() && ++npcLogCounter % 100 == 0) {
        // Count NPCs by state
        int idleCount = 0, alertCount = 0, engagedCount = 0, leasingCount = 0, fleeingCount = 0, deadCount = 0;
        for (const auto& [npcId, npc] : npcs_) {
            using NpcAiState = req::shared::data::NpcAiState;
            switch (npc.aiState) {
                case NpcAiState::Idle: idleCount++; break;
                case NpcAiState::Alert: alertCount++; break;
                case NpcAiState::Engaged: engagedCount++; break;
                case NpcAiState::Leashing: leasingCount++; break;
                case NpcAiState::Fleeing: fleeingCount++; break;
                case NpcAiState::Dead: deadCount++; break;
                default: break;
            }
        }
        
        req::shared::logInfo("zone", std::string{"[NPC] Active: "} + std::to_string(npcs_.size()) +
            " NPC(s) - Idle:" + std::to_string(idleCount) +
            ", Alert:" + std::to_string(alertCount) +
            ", Engaged:" + std::to_string(engagedCount) +
            ", Leashing:" + std::to_string(leasingCount) +
            ", Fleeing:" + std::to_string(fleeingCount) +
            ", Dead:" + std::to_string(deadCount));
    }
    
    // Process corpse decay (check every second = 20 ticks)
    static std::uint64_t corpseDecayCounter = 0;
    if (++corpseDecayCounter % 20 == 0 && !corpses_.empty()) {
        processCorpseDecay();
    }
}

void ZoneServer::broadcastSnapshots() {
    if (players_.empty()) {
        // No players to broadcast
        return;
    }
    
    // Log snapshot building (periodic, not every tick to reduce spam)
    static std::uint64_t logCounter = 0;
    bool doDetailedLog = (++logCounter % 20 == 0);  // Log every 20 snapshots (~1 second at 20Hz)
    
    if (doDetailedLog) {
        req::shared::logInfo("zone", std::string{"[Snapshot] Building snapshot "} + 
            std::to_string(snapshotCounter_ + 1) + " for " + std::to_string(players_.size()) + " active player(s)");
    }
    
    ++snapshotCounter_;
    
    // If broadcastFullState is true, use old behavior (single snapshot for all clients)
    if (zoneConfig_.broadcastFullState) {
        // Build single snapshot with ALL players
        req::shared::protocol::PlayerStateSnapshotData snapshot;
        snapshot.snapshotId = snapshotCounter_;
        
        for (const auto& [characterId, player] : players_) {
            if (!player.isInitialized) {
                continue;
            }
            
            req::shared::protocol::PlayerStateEntry entry;
            entry.characterId = player.characterId;
            entry.posX = player.posX;
            entry.posY = player.posY;
            entry.posZ = player.posZ;
            entry.velX = player.velX;
            entry.velY = player.velY;
            entry.velZ = player.velZ;
            entry.yawDegrees = player.yawDegrees;
            
            snapshot.players.push_back(entry);
            
            // Log what we're putting into the snapshot
            if (doDetailedLog) {
                req::shared::logInfo("zone", std::string{"[Snapshot] Adding entry: charId="} +
                    std::to_string(player.characterId) + 
                    ", pos=(" + std::to_string(entry.posX) + "," + std::to_string(entry.posY) + "," + std::to_string(entry.posZ) + ")" +
                    ", vel=(" + std::to_string(entry.velX) + "," + std::to_string(entry.velY) + "," + std::to_string(entry.velZ) + ")");
            }
        }
        
        // Build payload once
        std::string payloadStr = req::shared::protocol::buildPlayerStateSnapshotPayload(snapshot);
        req::shared::net::Connection::ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
        
        // Log the actual payload string
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Snapshot] Payload: '"} + payloadStr + "'");
        }
        
        // Broadcast to all connected clients (with safety checks)
        int sentCount = 0;
        int failedCount = 0;
        for (auto& connection : connections_) {
            if (!connection) {
                failedCount++;
                continue;
            }
            
            // Check if connection is closed
            if (connection->isClosed()) {
                failedCount++;
                continue;
            }
            
            try {
                connection->send(req::shared::MessageType::PlayerStateSnapshot, payloadBytes);
                sentCount++;
            } catch (const std::exception& e) {
                req::shared::logWarn("zone", std::string{"[Snapshot] Failed to send to connection: "} + e.what());
                failedCount++;
            }
        }
        
        // Log broadcast summary (periodic)
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Snapshot] Broadcast snapshot "} + 
                std::to_string(snapshot.snapshotId) + " with " + std::to_string(snapshot.players.size()) + 
                " player(s) to " + std::to_string(sentCount) + " connection(s) [FULL BROADCAST]" +
                (failedCount > 0 ? " (failed: " + std::to_string(failedCount) + ")" : ""));
        }
    } else {
        // Interest-based filtering: build per-recipient snapshots
        int totalSent = 0;
        int totalFailed = 0;
        
        for (auto& [recipientCharId, recipientPlayer] : players_) {
            if (!recipientPlayer.isInitialized || !recipientPlayer.connection) {
                continue;
            }
            
            // Check if connection is still valid
            if (recipientPlayer.connection->isClosed()) {
                continue;
            }
            
            // Build snapshot for this specific recipient
            req::shared::protocol::PlayerStateSnapshotData snapshot;
            snapshot.snapshotId = snapshotCounter_;
            
            float recipientX = recipientPlayer.posX;
            float recipientY = recipientPlayer.posY;
            float recipientZ = recipientPlayer.posZ;
            
            int includedCount = 0;
            
            for (const auto& [otherCharId, otherPlayer] : players_) {
                if (!otherPlayer.isInitialized) {
                    continue;
                }
                
                // Always include self
                if (otherCharId == recipientCharId) {
                    req::shared::protocol::PlayerStateEntry entry;
                    entry.characterId = otherPlayer.characterId;
                    entry.posX = otherPlayer.posX;
                    entry.posY = otherPlayer.posY;
                    entry.posZ = otherPlayer.posZ;
                    entry.velX = otherPlayer.velX;
                    entry.velY = otherPlayer.velY;
                    entry.velZ = otherPlayer.velZ;
                    entry.yawDegrees = otherPlayer.yawDegrees;
                    snapshot.players.push_back(entry);
                    includedCount++;
                    
                    // Log self entry
                    if (doDetailedLog) {
                        req::shared::logInfo("zone", std::string{"[Snapshot] For charId="} + std::to_string(recipientCharId) +
                            " adding SELF: pos=(" + std::to_string(entry.posX) + "," + std::to_string(entry.posY) + "," + std::to_string(entry.posZ) + ")");
                    }
                    continue;
                }
                
                // Check distance (2D or 3D - using 2D for now: XY plane)
                float dx = otherPlayer.posX - recipientX;
                float dy = otherPlayer.posY - recipientY;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                if (distance <= zoneConfig_.interestRadius) {
                    req::shared::protocol::PlayerStateEntry entry;
                    entry.characterId = otherPlayer.characterId;
                    entry.posX = otherPlayer.posX;
                    entry.posY = otherPlayer.posY;
                    entry.posZ = otherPlayer.posZ;
                    entry.velX = otherPlayer.velX;
                    entry.velY = otherPlayer.velY;
                    entry.velZ = otherPlayer.velZ;
                    entry.yawDegrees = otherPlayer.yawDegrees;
                    snapshot.players.push_back(entry);
                    includedCount++;
                }
            }
            
            // Debug logging (if enabled)
            if (zoneConfig_.debugInterest && doDetailedLog) {
                req::shared::logInfo("zone", std::string{"[Snapshot] (filtered) recipientCharId="} + 
                    std::to_string(recipientCharId) + ", playersIncluded=" + std::to_string(includedCount) +
                    " (out of " + std::to_string(players_.size()) + " total)");
            }
            
            // Build payload and send to this recipient (with error handling)
            try {
                std::string payloadStr = req::shared::protocol::buildPlayerStateSnapshotPayload(snapshot);
                req::shared::net::Connection::ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
                
                // Log payload for this recipient
                if (doDetailedLog) {
                    req::shared::logInfo("zone", std::string{"[Snapshot] For charId="} + std::to_string(recipientCharId) +
                        " payload: '" + payloadStr + "'");
                }

                recipientPlayer.connection->send(req::shared::MessageType::PlayerStateSnapshot, payloadBytes);
                totalSent++;
            } catch (const std::exception& e) {
                req::shared::logWarn("zone", std::string{"[Snapshot] Failed to send to charId="} + 
                    std::to_string(recipientCharId) + ": " + e.what());
                totalFailed++;
            }
        }
        
        // Log overall result (periodic)
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Snapshot] Finished sending filtered snapshots: "} +
                std::to_string(totalSent) + " sent, " + std::to_string(totalFailed) + " failed");
        }
    }
}

} // namespace req::zone
