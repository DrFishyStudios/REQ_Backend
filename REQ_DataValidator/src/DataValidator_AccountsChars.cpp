#include "../include/req/datavalidator/DataValidator.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

using req::shared::logInfo;
using req::shared::logWarn;
using req::shared::logError;

namespace req::datavalidator {

bool validateAccountsAndCharacters(const std::string& accountsRoot,
                                   const std::string& charactersRoot,
                                   const req::shared::WorldConfig& worldConfig,
                                   const std::vector<req::shared::ZoneConfig>& zoneConfigs)
{
    bool ok = true;

    std::unordered_set<std::uint64_t> accountIds;
    std::unordered_set<std::uint32_t> validZoneIds;
    validZoneIds.reserve(zoneConfigs.size());
    for (const auto& z : zoneConfigs) {
        validZoneIds.insert(z.zoneId);
    }

    // 1) Load all accounts
    try {
        if (!fs::exists(accountsRoot)) {
            logWarn("AccountValidation",
                    "Accounts directory does not exist, skipping account validation: " + accountsRoot);
        } else {
            for (const auto& entry : fs::directory_iterator(accountsRoot)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".json") continue;

                std::ifstream f(entry.path());
                if (!f.is_open()) {
                    logError("AccountValidation", "Failed to open account file: " + entry.path().string());
                    ok = false;
                    continue;
                }

                json j;
                try {
                    f >> j;
                } catch (const json::exception& e) {
                    logError("AccountValidation",
                             "Failed to parse account JSON in " + entry.path().string() + ": " + e.what());
                    ok = false;
                    continue;
                }

                req::shared::data::Account account = j.get<req::shared::data::Account>();
                if (account.accountId == 0) {
                    logError("AccountValidation",
                             "Account file " + entry.path().string() + " has account_id=0");
                    ok = false;
                } else {
                    accountIds.insert(account.accountId);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("AccountValidation",
                 "Filesystem error while loading accounts: " + std::string(e.what()));
        ok = false;
    }

    // 2) Load all characters and cross-validate
    try {
        if (!fs::exists(charactersRoot)) {
            logWarn("CharacterValidation",
                    "Characters directory does not exist, skipping character validation: " + charactersRoot);
            return ok;
        }

        for (const auto& entry : fs::directory_iterator(charactersRoot)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".json") continue;

            std::ifstream f(entry.path());
            if (!f.is_open()) {
                logError("CharacterValidation",
                         "Failed to open character file: " + entry.path().string());
                ok = false;
                continue;
            }

            json j;
            try {
                f >> j;
            } catch (const json::exception& e) {
                logError("CharacterValidation",
                         "Failed to parse character JSON in " + entry.path().string() + ": " + e.what());
                ok = false;
                continue;
            }

            req::shared::data::Character c = j.get<req::shared::data::Character>();

            // a) Character must reference an existing account (if any accounts exist)
            if (!accountIds.empty() && accountIds.find(c.accountId) == accountIds.end()) {
                logError("CharacterValidation",
                         "Character " + std::to_string(c.characterId) +
                         " ('" + c.name + "') references unknown accountId " +
                         std::to_string(c.accountId));
                ok = false;
            }

            // b) Check lastWorldId & lastZoneId basic sanity
            if (c.lastWorldId != 0 && static_cast<std::uint32_t>(c.lastWorldId) != worldConfig.worldId) {
                logWarn("CharacterValidation",
                        "Character " + std::to_string(c.characterId) +
                        " has lastWorldId=" + std::to_string(c.lastWorldId) +
                        " which does not match configured worldId=" +
                        std::to_string(worldConfig.worldId));
                // treat this as warning, not a hard failure
            }

            if (c.lastZoneId != 0 && !validZoneIds.empty() &&
                validZoneIds.find(static_cast<std::uint32_t>(c.lastZoneId)) == validZoneIds.end()) {
                logWarn("CharacterValidation",
                        "Character " + std::to_string(c.characterId) +
                        " has lastZoneId=" + std::to_string(c.lastZoneId) +
                        " which is not in configured zone set.");
                // warning, not fatal
            }

            // c) Position values finite
            auto isFinite = [](float v) {
                return std::isfinite(v);
            };

            if (!isFinite(c.positionX) || !isFinite(c.positionY) || !isFinite(c.positionZ)) {
                logError("CharacterValidation",
                         "Character " + std::to_string(c.characterId) +
                         " has non-finite position values (x,y,z).");
                ok = false;
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("CharacterValidation",
                 "Filesystem error while loading characters: " + std::string(e.what()));
        ok = false;
    }

    return ok;
}

} // namespace req::datavalidator
