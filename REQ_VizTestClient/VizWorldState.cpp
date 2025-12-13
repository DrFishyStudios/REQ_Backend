#include "VizWorldState.h"
#include <iostream>

// DEBUG: Counter for throttled NPC spawn logging
static int g_debugNpcSpawnCount = 0;
constexpr int MAX_DEBUG_NPC_SPAWNS = 10;

void VizWorldState::applyPlayerStateSnapshot(
    const req::shared::protocol::PlayerStateSnapshotData& snapshot)
{
    // Only players are in this snapshot; NPCs come via EntitySpawn/Update.
    for (const auto& player : snapshot.players)
    {
        VizEntity& e = getOrCreateEntity(player.characterId, false);
        e.posX = player.posX;
        e.posY = player.posY;
        e.posZ = player.posZ;
        e.isNpc = false;
        e.isLocalPlayer = (player.characterId == localCharacterId_);
        // We don’t get HP or name here; those will come from other messages if needed.
    }
}

void VizWorldState::applyEntitySpawn(
    const req::shared::protocol::EntitySpawnData& spawn)
{
    const bool isNpc = (spawn.entityType == 1);

    // DEBUG: Check if entity already exists
    bool entityExists = (entities_.find(spawn.entityId) != entities_.end());
    std::string action = entityExists ? "OVERWRITE" : "INSERT";

    VizEntity& e = getOrCreateEntity(spawn.entityId, isNpc);
    e.isNpc = isNpc;
    e.posX = spawn.posX;
    e.posY = spawn.posY;
    e.posZ = spawn.posZ;
    e.hp = spawn.hp;
    e.maxHp = spawn.maxHp;
    e.name = spawn.name;
    
    // DEBUG: Log for tracked NPC (entityId >= 1 && entityId <= 10 for first few NPCs)
    if (isNpc && spawn.entityId <= 10) {
        std::cout << "[WORLDSTATE-APPLY] " << action << " entityId=" << spawn.entityId
                 << ", pos=(" << e.posX << "," << e.posY << "," << e.posZ << ")"
                 << ", hp=" << e.hp << "/" << e.maxHp
                 << ", name=\"" << e.name << "\"\n";
    }
    
    // DEBUG: Log first N NPC spawns
    if (isNpc && g_debugNpcSpawnCount < MAX_DEBUG_NPC_SPAWNS) {
        std::cout << "[DEBUG-VizWorldState] NPC Spawn #" << g_debugNpcSpawnCount << ": "
                 << "entityId=" << spawn.entityId
                 << ", name=\"" << spawn.name << "\""
                 << ", pos=(" << spawn.posX << "," << spawn.posY << "," << spawn.posZ << ")"
                 << ", hp=" << spawn.hp << "/" << spawn.maxHp
                 << ", level=" << spawn.level
                 << ", isNpc=" << (e.isNpc ? "true" : "false")
                 << "\n";
        g_debugNpcSpawnCount++;
    }
}

void VizWorldState::applyEntityUpdate(
    const req::shared::protocol::EntityUpdateData& update)
{
    // NPCs are the ones using EntityUpdate; players use PlayerStateSnapshot.
    VizEntity& e = getOrCreateEntity(update.entityId, true);
    e.posX = update.posX;
    e.posY = update.posY;
    e.posZ = update.posZ;
    e.hp = update.hp;
    e.state = update.state;
    
    // DEBUG: Log when HP reaches 0 (death)
    if (update.hp <= 0) {
        std::cout << "[VizWorldState] Entity " << update.entityId 
                 << " updated with HP=0 (dead), state=" << static_cast<int>(update.state) << "\n";
    }
}

void VizWorldState::applyEntityDespawn(
    const req::shared::protocol::EntityDespawnData& despawn)
{
    auto it = entities_.find(despawn.entityId);
    if (it != entities_.end()) {
        std::cout << "[VizWorldState] Removing entity " << despawn.entityId 
                 << " (reason=" << despawn.reason << ")\n";
        entities_.erase(despawn.entityId);
    } else {
        std::cout << "[VizWorldState] EntityDespawn for unknown entity " << despawn.entityId << "\n";
    }
}

VizEntity& VizWorldState::getOrCreateEntity(std::uint64_t id, bool isNpc)
{
    auto it = entities_.find(id);
    if (it == entities_.end())
    {
        VizEntity e;
        e.entityId = id;
        e.isNpc = isNpc;
        auto [insertIt, _] = entities_.emplace(id, std::move(e));
        return insertIt->second;
    }
    return it->second;
}
