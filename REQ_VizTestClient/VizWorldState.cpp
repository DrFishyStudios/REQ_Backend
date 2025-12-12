#include "VizWorldState.h"

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

    VizEntity& e = getOrCreateEntity(spawn.entityId, isNpc);
    e.isNpc = isNpc;
    e.posX = spawn.posX;
    e.posY = spawn.posY;
    e.posZ = spawn.posZ;
    e.hp = spawn.hp;
    e.maxHp = spawn.maxHp;
    e.name = spawn.name;
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
}

void VizWorldState::applyEntityDespawn(
    const req::shared::protocol::EntityDespawnData& despawn)
{
    entities_.erase(despawn.entityId);
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
