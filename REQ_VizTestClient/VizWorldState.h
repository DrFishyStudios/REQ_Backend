#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <req/shared/ProtocolSchemas.h>

// Simple data we need for drawing an entity in the viz client.
struct VizEntity
{
    std::uint64_t entityId{ 0 };
    bool isNpc{ false };
    bool isLocalPlayer{ false };

    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };

    std::int32_t hp{ 0 };
    std::int32_t maxHp{ 0 };

    std::uint8_t state{ 0 };
    std::string name;
};

class VizWorldState
{
public:
    VizWorldState() = default;

    void setLocalCharacterId(std::uint64_t characterId) { localCharacterId_ = characterId; }

    // Feed data in from protocol structs
    void applyPlayerStateSnapshot(const req::shared::protocol::PlayerStateSnapshotData& snapshot);
    void applyEntitySpawn(const req::shared::protocol::EntitySpawnData& spawn);
    void applyEntityUpdate(const req::shared::protocol::EntityUpdateData& update);
    void applyEntityDespawn(const req::shared::protocol::EntityDespawnData& despawn);

    const std::unordered_map<std::uint64_t, VizEntity>& getEntities() const { return entities_; }

private:
    std::uint64_t localCharacterId_{ 0 };
    std::unordered_map<std::uint64_t, VizEntity> entities_;

    VizEntity& getOrCreateEntity(std::uint64_t id, bool isNpc);
};
