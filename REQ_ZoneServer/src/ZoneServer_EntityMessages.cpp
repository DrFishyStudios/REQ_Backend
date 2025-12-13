#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <cmath>
#include <unordered_map>

// DEBUG: Throttled entity send logging (per-client tracking)
namespace {
    struct ClientEntitySendTracker {
        int entitySpawnCount = 0;
        int entityUpdateCount = 0;
        int entityDespawnCount = 0;
        static constexpr int MAX_DEBUG_SENDS_PER_TYPE = 20;
    };
    
    std::unordered_map<void*, ClientEntitySendTracker> g_clientTrackers;
    
    ClientEntitySendTracker& GetTracker(void* connectionPtr) {
        return g_clientTrackers[connectionPtr];
    }
}

namespace req::zone {

// ============================================================================
// Entity Spawn Messages
// ============================================================================

void ZoneServer::sendEntitySpawn(ConnectionPtr connection, std::uint64_t entityId) {
    if (!connection) {
        req::shared::logWarn("zone", "[ENTITY_SPAWN] Null connection, cannot send spawn message");
        return;
    }
    
    // DEBUG: Throttled logging per client
    auto& tracker = GetTracker(connection.get());
    bool shouldLog = (tracker.entitySpawnCount < ClientEntitySendTracker::MAX_DEBUG_SENDS_PER_TYPE);
    
    // Check if entity is a player
    auto playerIt = players_.find(entityId);
    if (playerIt != players_.end()) {
        const ZonePlayer& player = playerIt->second;
        
        // Build EntitySpawn message for player
        req::shared::protocol::EntitySpawnData spawnData;
        spawnData.entityId = player.characterId;
        spawnData.entityType = 0;  // 0 = Player
        spawnData.templateId = 0;  // TODO: Use race ID from character
        spawnData.name = "Player_" + std::to_string(player.characterId);  // TODO: Load actual name from character
        spawnData.posX = player.posX;
        spawnData.posY = player.posY;
        spawnData.posZ = player.posZ;
        spawnData.heading = player.yawDegrees;
        spawnData.level = player.level;
        spawnData.hp = player.hp;
        spawnData.maxHp = player.maxHp;
        
        std::string payload = req::shared::protocol::buildEntitySpawnPayload(spawnData);
        req::shared::net::Connection::ByteArray payloadBytes(payload.begin(), payload.end());
        connection->send(req::shared::MessageType::EntitySpawn, payloadBytes);
        
        if (shouldLog) {
            req::shared::logInfo("zone", std::string{"[ENTITY_SPAWN #"} + std::to_string(tracker.entitySpawnCount) + 
                "] Sent to client: type=44, entityId=" + std::to_string(entityId) +
                ", entityType=0 (Player), payloadSize=" + std::to_string(payload.size()));
            tracker.entitySpawnCount++;
        }
        return;
    }
    
    // Check if entity is an NPC
    auto npcIt = npcs_.find(entityId);
    if (npcIt != npcs_.end()) {
        const auto& npc = npcIt->second;
        
        // Build EntitySpawn message for NPC
        req::shared::protocol::EntitySpawnData spawnData;
        spawnData.entityId = npc.npcId;
        spawnData.entityType = 1;  // 1 = NPC
        spawnData.templateId = npc.templateId;
        spawnData.name = npc.name;
        spawnData.posX = npc.posX;
        spawnData.posY = npc.posY;
        spawnData.posZ = npc.posZ;
        spawnData.heading = npc.facingDegrees;
        spawnData.level = npc.level;
        spawnData.hp = npc.currentHp;
        spawnData.maxHp = npc.maxHp;
        
        std::string payload = req::shared::protocol::buildEntitySpawnPayload(spawnData);
        req::shared::net::Connection::ByteArray payloadBytes(payload.begin(), payload.end());
        connection->send(req::shared::MessageType::EntitySpawn, payloadBytes);
        
        if (shouldLog) {
            req::shared::logInfo("zone", std::string{"[ENTITY_SPAWN #"} + std::to_string(tracker.entitySpawnCount) + 
                "] Sent to client: type=44, entityId=" + std::to_string(entityId) +
                ", entityType=1 (NPC), name=\"" + npc.name + "\", payloadSize=" + std::to_string(payload.size()));
            tracker.entitySpawnCount++;
        }
        return;
    }
    
    req::shared::logWarn("zone", std::string{"[ENTITY_SPAWN] Entity not found: entityId="} +
        std::to_string(entityId));
}

void ZoneServer::broadcastEntitySpawn(std::uint64_t entityId) {
    req::shared::logInfo("zone", std::string{"[ENTITY_SPAWN] Broadcasting spawn: entityId="} +
        std::to_string(entityId));
    
    // Send to all connected players
    for (auto& [characterId, player] : players_) {
        if (!player.connection || !player.isInitialized) {
            continue;
        }
        
        // Don't send player their own spawn
        if (characterId == entityId) {
            continue;
        }
        
        // Mark entity as known to this player
        player.knownEntities.insert(entityId);
        
        // Send spawn message
        sendEntitySpawn(player.connection, entityId);
    }
}

void ZoneServer::sendAllKnownEntities(ConnectionPtr connection, std::uint64_t characterId) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        req::shared::logWarn("zone", "[ENTITY_SPAWN] Player not found for sendAllKnownEntities");
        return;
    }
    
    ZonePlayer& player = playerIt->second;
    
    req::shared::logInfo("zone", std::string{"[ENTITY_SPAWN] Sending all known entities to characterId="} +
        std::to_string(characterId) + " (players=" + std::to_string(players_.size() - 1) +
        ", npcs=" + std::to_string(npcs_.size()) + ")");
    
    // Send all other players
    for (const auto& [otherCharId, otherPlayer] : players_) {
        if (otherCharId == characterId || !otherPlayer.isInitialized) {
            continue;
        }
        
        player.knownEntities.insert(otherCharId);
        sendEntitySpawn(connection, otherCharId);
    }
    
    // Send all NPCs
    for (const auto& [npcId, npc] : npcs_) {
        if (!npc.isAlive) {
            continue;  // Don't send dead NPCs
        }
        
        player.knownEntities.insert(npcId);
        sendEntitySpawn(connection, npcId);
    }
}

// ============================================================================
// Entity Update Messages
// ============================================================================

void ZoneServer::sendEntityUpdate(ConnectionPtr connection, std::uint64_t entityId) {
    if (!connection) {
        return;
    }
    
    // Check if entity is an NPC (players use PlayerStateSnapshot)
    auto npcIt = npcs_.find(entityId);
    if (npcIt != npcs_.end()) {
        const auto& npc = npcIt->second;
        
        // Build EntityUpdate message for NPC
        req::shared::protocol::EntityUpdateData updateData;
        updateData.entityId = npc.npcId;
        updateData.posX = npc.posX;
        updateData.posY = npc.posY;
        updateData.posZ = npc.posZ;
        updateData.heading = npc.facingDegrees;
        updateData.hp = npc.currentHp;
        updateData.state = static_cast<std::uint8_t>(npc.aiState);
        
        std::string payload = req::shared::protocol::buildEntityUpdatePayload(updateData);
        req::shared::net::Connection::ByteArray payloadBytes(payload.begin(), payload.end());
        connection->send(req::shared::MessageType::EntityUpdate, payloadBytes);
    }
}

void ZoneServer::broadcastEntityUpdates() {
    // Send NPC updates to all players
    for (auto& [characterId, player] : players_) {
        if (!player.connection || !player.isInitialized) {
            continue;
        }
        
        // Send updates for NPCs this player knows about
        for (std::uint64_t entityId : player.knownEntities) {
            // Skip player entities (they use PlayerStateSnapshot)
            if (players_.find(entityId) != players_.end()) {
                continue;
            }
            
            // Only send updates for NPCs
            auto npcIt = npcs_.find(entityId);
            if (npcIt != npcs_.end() && npcIt->second.isAlive) {
                sendEntityUpdate(player.connection, entityId);
            }
        }
    }
}

// ============================================================================
// Entity Despawn Messages
// ============================================================================

void ZoneServer::sendEntityDespawn(ConnectionPtr connection, std::uint64_t entityId, std::uint32_t reason) {
    if (!connection) {
        return;
    }
    
    req::shared::protocol::EntityDespawnData despawnData;
    despawnData.entityId = entityId;
    despawnData.reason = reason;
    
    std::string payload = req::shared::protocol::buildEntityDespawnPayload(despawnData);
    req::shared::net::Connection::ByteArray payloadBytes(payload.begin(), payload.end());
    connection->send(req::shared::MessageType::EntityDespawn, payloadBytes);
    
    req::shared::logInfo("zone", std::string{"[ENTITY_DESPAWN] Sent despawn: entityId="} +
        std::to_string(entityId) + ", reason=" + std::to_string(reason));
}

void ZoneServer::broadcastEntityDespawn(std::uint64_t entityId, std::uint32_t reason) {
    req::shared::logInfo("zone", std::string{"[ENTITY_DESPAWN] Broadcasting despawn: entityId="} +
        std::to_string(entityId) + ", reason=" + std::to_string(reason));
    
    // Send to all connected players who know about this entity
    for (auto& [characterId, player] : players_) {
        if (!player.connection || !player.isInitialized) {
            continue;
        }
        
        // Check if player knows about this entity
        if (player.knownEntities.find(entityId) != player.knownEntities.end()) {
            // Remove from known entities
            player.knownEntities.erase(entityId);
            
            // Send despawn message
            sendEntityDespawn(player.connection, entityId, reason);
        }
    }
}

} // namespace req::zone
