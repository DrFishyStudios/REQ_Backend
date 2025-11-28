#include "../include/req/shared/DataLoader.h"

namespace req::shared::data {

std::vector<ItemDef> loadItems(const std::string& path) {
    (void)path; // suppress unused
    // TODO: Read from data/core/items.json and parse into ItemDef list
    return {};
}

std::vector<NpcTemplate> loadNpcs(const std::string& path) {
    (void)path; // suppress unused
    // TODO: Read from data/core/npcs.json and parse into NpcTemplate list
    return {};
}

} // namespace req::shared::data
