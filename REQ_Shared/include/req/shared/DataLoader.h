#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "DataModels.h"

namespace req::shared::data {

std::vector<ItemDef> loadItems(const std::string& path);
std::vector<NpcTemplate> loadNpcs(const std::string& path);

} // namespace req::shared::data
