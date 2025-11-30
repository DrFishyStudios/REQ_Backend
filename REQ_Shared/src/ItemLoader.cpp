#include "../include/req/shared/ItemLoader.h"
#include "../include/req/shared/Logger.h"

#include "../thirdparty/nlohmann/json.hpp"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace req::shared::data {

ItemTemplateMap loadItemTemplates(const std::string& path) {
    ItemTemplateMap map;

    logInfo("ItemLoader", "Loading item templates from: " + path);

    if (!fs::exists(path)) {
        logWarn("ItemLoader", "Items file does not exist: " + path);
        return map;
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        logError("ItemLoader", "Failed to open items file: " + path);
        return map;
    }

    json j;
    try {
        f >> j;
    } catch (const json::exception& e) {
        logError("ItemLoader",
                 std::string{"Failed to parse items JSON from "} + path + ": " + e.what());
        return map;
    }

    if (!j.contains("items") || !j["items"].is_array()) {
        logError("ItemLoader", "Items file missing 'items' array: " + path);
        return map;
    }

    for (const auto& itemJson : j["items"]) {
        try {
            ItemTemplate item = itemJson.get<ItemTemplate>();
            if (item.itemId == 0) {
                logError("ItemLoader", "Item with item_id=0 in " + path);
                continue;
            }
            if (!map.emplace(item.itemId, std::move(item)).second) {
                logError("ItemLoader",
                         "Duplicate item_id in items file: " + std::to_string(item.itemId));
            }
        } catch (const std::exception& e) {
            logError("ItemLoader",
                     std::string{"Exception while parsing item in "} + path + ": " + e.what());
        }
    }

    logInfo("ItemLoader", "Loaded " + std::to_string(map.size()) + " item templates.");
    return map;
}

LootTableMap loadLootTablesFromZoneFile(const std::string& path, std::uint32_t& outZoneId) {
    LootTableMap map;
    outZoneId = 0;

    logInfo("ItemLoader", "Loading zone loot from: " + path);

    if (!fs::exists(path)) {
        logWarn("ItemLoader", "Zone loot file does not exist: " + path);
        return map;
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        logError("ItemLoader", "Failed to open zone loot file: " + path);
        return map;
    }

    json j;
    try {
        f >> j;
    } catch (const json::exception& e) {
        logError("ItemLoader",
                 std::string{"Failed to parse zone loot JSON from "} + path + ": " + e.what());
        return map;
    }

    outZoneId = j.value("zone_id", 0u);

    if (!j.contains("loot_tables") || !j["loot_tables"].is_array()) {
        logError("ItemLoader", "Zone loot file missing 'loot_tables' array: " + path);
        return map;
    }

    for (const auto& tableJson : j["loot_tables"]) {
        try {
            LootTable table = tableJson.get<LootTable>();
            if (table.lootTableId == 0) {
                logError("ItemLoader", "LootTable with loot_table_id=0 in " + path);
                continue;
            }
            if (!map.emplace(table.lootTableId, std::move(table)).second) {
                logError("ItemLoader",
                         "Duplicate loot_table_id in zone loot file: " +
                         std::to_string(table.lootTableId));
            }
        } catch (const std::exception& e) {
            logError("ItemLoader",
                     std::string{"Exception while parsing loot table in "} + path + ": " + e.what());
        }
    }

    logInfo("ItemLoader", "Loaded " + std::to_string(map.size()) +
                          " loot tables from zone loot file.");
    return map;
}

} // namespace req::shared::data
