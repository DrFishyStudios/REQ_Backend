#include "../include/req/datavalidator/DataValidator.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"

#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

using req::shared::logInfo;
using req::shared::logWarn;
using req::shared::logError;

namespace req::datavalidator {

bool validateNpcData(const std::string& zonesRoot) {
    bool ok = true;

    try {
        if (!fs::exists(zonesRoot)) {
            logWarn("NpcValidation", "Zones directory does not exist for NPC data: " + zonesRoot);
            return ok;
        }

        for (const auto& entry : fs::directory_iterator(zonesRoot)) {
            if (!entry.is_regular_file()) continue;
            const auto path = entry.path();
            if (path.extension() != ".json") continue;

            const auto filename = path.filename().string();
            if (filename.find("zone_") != 0 || filename.find("_npcs.json") == std::string::npos) {
                continue;
            }

            logInfo("NpcValidation", "Validating NPC data file: " + filename);

            std::ifstream f(path);
            if (!f.is_open()) {
                logError("NpcValidation", "Failed to open NPC file: " + path.string());
                ok = false;
                continue;
            }

            json j;
            try {
                f >> j;
            } catch (const json::exception& e) {
                logError("NpcValidation",
                         "Failed to parse JSON for NPC file " + path.string() + ": " + e.what());
                ok = false;
                continue;
            }

            if (!j.contains("npcs") || !j["npcs"].is_array()) {
                logError("NpcValidation", "NPC file missing 'npcs' array: " + path.string());
                ok = false;
                continue;
            }

            std::unordered_set<std::uint32_t> npcIds;
            for (const auto& npcJson : j["npcs"]) {
                auto npcId = npcJson.value("npc_id", 0u);
                auto name  = npcJson.value("name", std::string{});

                if (npcId == 0) {
                    logError("NpcValidation", "NPC with npc_id=0 in " + path.string());
                    ok = false;
                }

                if (!npcIds.insert(npcId).second) {
                    logError("NpcValidation",
                             "Duplicate npc_id " + std::to_string(npcId) +
                             " in file: " + path.string());
                    ok = false;
                }

                auto level       = npcJson.value("level", 0);
                auto maxHp       = npcJson.value("max_hp", 0);
                auto aggroRadius = npcJson.value("aggro_radius", 0.0f);
                auto leashRadius = npcJson.value("leash_radius", 0.0f);

                if (level <= 0) {
                    logError("NpcValidation",
                             "NPC " + std::to_string(npcId) + " ('" + name +
                             "') has invalid level: " + std::to_string(level));
                    ok = false;
                }

                if (maxHp <= 0) {
                    logError("NpcValidation",
                             "NPC " + std::to_string(npcId) + " ('" + name +
                             "') has invalid max_hp: " + std::to_string(maxHp));
                    ok = false;
                }

                if (aggroRadius <= 0.0f) {
                    logError("NpcValidation",
                             "NPC " + std::to_string(npcId) + " ('" + name +
                             "') has invalid aggro_radius: " + std::to_string(aggroRadius));
                    ok = false;
                }

                if (leashRadius <= 0.0f) {
                    logError("NpcValidation",
                             "NPC " + std::to_string(npcId) + " ('" + name +
                             "') has invalid leash_radius: " + std::to_string(leashRadius));
                    ok = false;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("NpcValidation",
                 "Filesystem error while validating NPC data: " + std::string(e.what()));
        ok = false;
    }

    return ok;
}

} // namespace req::datavalidator
