#include "req/clientcore/ClientCore.h"

#include "../../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::clientcore {

// These are thin wrappers around REQ_Shared protocol parsers

bool parsePlayerStateSnapshot(
    const std::string& payload,
    req::shared::protocol::PlayerStateSnapshotData& outData) {
    
    return req::shared::protocol::parsePlayerStateSnapshotPayload(payload, outData);
}

bool parseAttackResult(
    const std::string& payload,
    req::shared::protocol::AttackResultData& outData) {
    
    return req::shared::protocol::parseAttackResultPayload(payload, outData);
}

bool parseDevCommandResponse(
    const std::string& payload,
    req::shared::protocol::DevCommandResponseData& outData) {
    
    return req::shared::protocol::parseDevCommandResponsePayload(payload, outData);
}

bool parseEntitySpawn(
    const std::string& payload,
    req::shared::protocol::EntitySpawnData& outData) {
    
    return req::shared::protocol::parseEntitySpawnPayload(payload, outData);
}

bool parseEntityUpdate(
    const std::string& payload,
    req::shared::protocol::EntityUpdateData& outData) {
    
    return req::shared::protocol::parseEntityUpdatePayload(payload, outData);
}

bool parseEntityDespawn(
    const std::string& payload,
    req::shared::protocol::EntityDespawnData& outData) {
    
    return req::shared::protocol::parseEntityDespawnPayload(payload, outData);
}

} // namespace req::clientcore
