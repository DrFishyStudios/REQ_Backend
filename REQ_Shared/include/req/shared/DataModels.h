#pragma once

#include <string>
#include <cstdint>

#include "Types.h"

namespace req::shared::data {

struct PlayerCore {
    PlayerId id { InvalidPlayerId };
    AccountId accountId { InvalidAccountId };
    std::string name;
    std::uint32_t level { 1 };
    WorldId worldId { InvalidWorldId };
    ZoneId lastZoneId { InvalidZoneId };
};

struct ItemDef {
    std::uint32_t id { 0 };
    std::string name;
    std::uint32_t iconId { 0 };
    std::uint32_t rarity { 0 };
    std::uint32_t maxStack { 1 };
    // TODO: Add more item attributes later
};

struct NpcTemplate {
    std::uint32_t id { 0 };
    std::string name;
    std::uint32_t level { 1 };
    std::uint32_t factionId { 0 };
    std::uint32_t lootTableId { 0 };
    // TODO: Add stats, behaviors, abilities
};

} // namespace req::shared::data
