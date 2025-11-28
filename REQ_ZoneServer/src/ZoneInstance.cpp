#include "../include/req/zone/ZoneInstance.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataLoader.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

namespace req::zone {

ZoneInstance::ZoneInstance(req::shared::ZoneId id, std::string name)
    : id_(id), name_(std::move(name))
{
    req::shared::logInfo("zone", "ZoneInstance created (stub)");
}

void ZoneInstance::loadStaticData() {
    // TODO: Load zone-specific static data (spawns, navmesh, etc.)
    req::shared::logInfo("zone", "ZoneInstance::loadStaticData called (stub)");
}

void ZoneInstance::update(float deltaSeconds) {
    (void)deltaSeconds; // suppress unused for now
    // TODO: Advance simulation tick, process entities
    req::shared::logInfo("zone", "ZoneInstance::update called (stub)");
}

} // namespace req::zone
