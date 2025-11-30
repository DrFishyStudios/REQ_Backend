#pragma once

#include <string>
#include <unordered_map>

#include "DataModels.h"

namespace req::shared::data {

// itemId -> ItemTemplate
using ItemTemplateMap = std::unordered_map<std::uint32_t, ItemTemplate>;

// lootTableId -> LootTable
using LootTableMap = std::unordered_map<std::uint32_t, LootTable>;

// Load all item templates from a JSON file
// Expected format: { "items": [ ... ] }
ItemTemplateMap loadItemTemplates(const std::string& path);

// Load loot tables from a zone-specific loot file
// Expected format: { "zone_id": N, "loot_tables": [ ... ], "npc_loot": [ ... ] }
// Returns the loot tables and sets outZoneId to the zone_id from the file
LootTableMap loadLootTablesFromZoneFile(const std::string& path, std::uint32_t& outZoneId);

} // namespace req::shared::data
