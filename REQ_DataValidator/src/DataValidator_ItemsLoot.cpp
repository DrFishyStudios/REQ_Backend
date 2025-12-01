#include "../include/req/datavalidator/DataValidator.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/ItemLoader.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

using req::shared::logInfo;
using req::shared::logWarn;
using req::shared::logError;

namespace req::datavalidator {

bool validateItemsAndLoot(const std::string& itemsRoot,
                          const std::string& lootRoot)
{
    bool ok = true;

    const std::string itemsPath = itemsRoot + "/items.json";

    // 1) Load item templates
    req::shared::data::ItemTemplateMap items =
        req::shared::data::loadItemTemplates(itemsPath);

    if (items.empty()) {
        logWarn("ItemsValidation",
                "No items loaded from " + itemsPath +
                " (items map is empty).");
        // Not necessarily fatal; could be a brand new DB with no items yet.
    }

    // 2) Iterate zone loot files
    try {
        if (!fs::exists(lootRoot)) {
            logWarn("ItemsValidation",
                    "Loot directory does not exist: " + lootRoot);
            return ok;
        }

        for (const auto& entry : fs::directory_iterator(lootRoot)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".json") continue;

            const auto filename = entry.path().filename().string();
            if (filename.find("zone_") != 0 || filename.find("_loot.json") == std::string::npos) {
                continue;
            }

            std::uint32_t zoneId = 0;
            req::shared::data::LootTableMap lootTables =
                req::shared::data::loadLootTablesFromZoneFile(entry.path().string(),
                                                              zoneId);

            if (lootTables.empty()) {
                logWarn("ItemsValidation",
                        "No loot tables found in " + entry.path().string());
                continue;
            }

            for (const auto& [tableId, table] : lootTables) {
                for (const auto& e : table.entries) {
                    if (e.itemId == 0) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " in file " + entry.path().string() +
                            " has entry with item_id=0");
                        ok = false;
                        continue;
                    }

                    if (items.find(e.itemId) == items.end()) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " (zone_id=" + std::to_string(zoneId) +
                            ") references unknown item_id=" + std::to_string(e.itemId));
                        ok = false;
                    }

                    if (e.chance < 0.0f || e.chance > 1.0f) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " (zone_id=" + std::to_string(zoneId) +
                            ") has invalid chance " + std::to_string(e.chance) +
                            " for item_id=" + std::to_string(e.itemId));
                        ok = false;
                    }

                    if (e.minCount == 0 || e.minCount > e.maxCount) {
                        logError("ItemsValidation",
                            "LootTable " + std::to_string(tableId) +
                            " (zone_id=" + std::to_string(zoneId) +
                            ") has invalid min/max count for item_id=" +
                            std::to_string(e.itemId) +
                            " (min=" + std::to_string(e.minCount) +
                            ", max=" + std::to_string(e.maxCount) + ")");
                        ok = false;
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("ItemsValidation",
                 "Filesystem error while validating items/loot: " +
                 std::string(e.what()));
        ok = false;
    }

    return ok;
}

} // namespace req::datavalidator
