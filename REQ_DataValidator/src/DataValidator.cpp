#include "../include/req/datavalidator/DataValidator.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <exception>

#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

using req::shared::logInfo;
using req::shared::logWarn;
using req::shared::logError;

namespace req::datavalidator {

namespace {

bool validateConfigs(const std::string& configRoot,
                     req::shared::WorldConfig& outWorldConfig,
                     std::vector<req::shared::ZoneConfig>& outZoneConfigs);

bool validateNpcData(const std::string& zonesRoot);

bool validateWorldRules(const std::string& configRoot,
                        const req::shared::WorldConfig& worldConfig);

bool validateAccountsAndCharacters(const std::string& accountsRoot,
                                   const std::string& charactersRoot,
                                   const req::shared::WorldConfig& worldConfig,
                                   const std::vector<req::shared::ZoneConfig>& zoneConfigs);

bool validateConfigs(const std::string& configRoot,
                     req::shared::WorldConfig& outWorldConfig,
                     std::vector<req::shared::ZoneConfig>& outZoneConfigs)
{
    bool ok = true;

    const std::string loginPath  = configRoot + "/login_config.json";
    const std::string worldPath  = configRoot + "/world_config.json";
    const std::string worldsPath = configRoot + "/worlds.json";
    const std::string zonesRoot  = configRoot + "/zones";

    // 1) Login config
    try {
        auto loginCfg = req::shared::loadLoginConfig(loginPath);
        // loadLoginConfig already validates port; no extra checks needed here.
        logInfo("ConfigValidation", "LoginConfig OK: " + loginCfg.address + ":" + std::to_string(loginCfg.port));
    } catch (const std::exception& e) {
        logError("ConfigValidation", std::string{"LoginConfig validation failed: "} + e.what());
        ok = false;
    }

    // 2) World config
    try {
        outWorldConfig = req::shared::loadWorldConfig(worldPath);
    } catch (const std::exception& e) {
        logError("ConfigValidation", std::string{"WorldConfig validation failed: "} + e.what());
        ok = false;
        // Without a world config, we can't do any of the deeper checks.
        return ok;
    }

    // Optional: check worldConfig against worlds.json if present
    try {
        if (fs::exists(worldsPath)) {
            std::ifstream wf(worldsPath);
            json wj;
            wf >> wj;

            if (!wj.contains("worlds") || !wj["worlds"].is_array()) {
                logWarn("ConfigValidation", "worlds.json does not contain 'worlds' array.");
            } else {
                std::unordered_set<std::uint32_t> worldIds;
                std::unordered_set<std::uint16_t> worldPorts;

                for (const auto& entry : wj["worlds"]) {
                    auto id   = entry.value("world_id", 0u);
                    auto port = entry.value("port", static_cast<std::uint16_t>(0));
                    std::string name = entry.value("world_name", std::string{});

                    if (id == 0) {
                        logError("ConfigValidation", "worlds.json entry has invalid world_id=0");
                        ok = false;
                    }

                    if (!worldIds.insert(id).second) {
                        logError("ConfigValidation", "Duplicate world_id in worlds.json: " + std::to_string(id));
                        ok = false;
                    }

                    if (port == 0 || port > 65535) {
                        logError("ConfigValidation",
                                 "Invalid port in worlds.json for world '" + name + "': " + std::to_string(port));
                        ok = false;
                    }

                    if (!worldPorts.insert(port).second) {
                        logError("ConfigValidation",
                                 "Duplicate world port in worlds.json: " + std::to_string(port));
                        ok = false;
                    }
                }
            }
        } else {
            logWarn("ConfigValidation", "worlds.json not found; skipping world list validation.");
        }
    } catch (const std::exception& e) {
        logError("ConfigValidation", std::string{"Exception while validating worlds.json: "} + e.what());
        ok = false;
    }

    // 3) Zone configs under config/zones/zone_*_config.json
    outZoneConfigs.clear();
    try {
        if (!fs::exists(zonesRoot)) {
            logWarn("ConfigValidation", "Zones config directory does not exist: " + zonesRoot);
            return ok;
        }

        std::unordered_set<std::uint32_t> zoneIds;
        std::unordered_set<std::uint16_t> zonePorts;

        for (const auto& entry : fs::directory_iterator(zonesRoot)) {
            if (!entry.is_regular_file()) continue;
            const auto path = entry.path();
            if (path.extension() != ".json") continue;
            const auto filename = path.filename().string();
            if (filename.find("zone_") != 0 || filename.find("_config.json") == std::string::npos) {
                continue;
            }

            try {
                auto zoneCfg = req::shared::loadZoneConfig(path.string());
                outZoneConfigs.push_back(zoneCfg);

                // Unique zone_id per config file
                if (!zoneIds.insert(zoneCfg.zoneId).second) {
                    logError("ConfigValidation",
                             "Duplicate zone_id across zone config files: " + std::to_string(zoneCfg.zoneId));
                    ok = false;
                }

                // Check the zone port specified in world_config
                for (const auto& zEntry : outWorldConfig.zones) {
                    if (zEntry.zoneId == zoneCfg.zoneId) {
                        if (zEntry.port == 0 || zEntry.port > 65535) {
                            logError("ConfigValidation",
                                     "Invalid zone port in world_config for zone "
                                     + std::to_string(zEntry.zoneId) + ": " + std::to_string(zEntry.port));
                            ok = false;
                        }
                        if (!zonePorts.insert(zEntry.port).second) {
                            logError("ConfigValidation",
                                     "Duplicate zone port in world_config: " + std::to_string(zEntry.port));
                            ok = false;
                        }
                    }
                }
            } catch (const std::exception& e) {
                logError("ConfigValidation",
                         "ZoneConfig validation failed for " + path.string() + ": " + e.what());
                ok = false;
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("ConfigValidation",
                 "Filesystem error while iterating zone configs: " + std::string(e.what()));
        ok = false;
    }

    return ok;
}

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

bool validateWorldRules(const std::string& configRoot,
                        const req::shared::WorldConfig& worldConfig)
{
    bool ok = true;

    const std::string rulesPath =
        configRoot + "/world_rules_" + worldConfig.rulesetId + ".json";

    try {
        logInfo("WorldRulesValidation", "Loading WorldRules from: " + rulesPath);
        auto rules = req::shared::loadWorldRules(rulesPath);

        // Ensure ruleset_id matches world_config.json ruleset_id
        if (rules.rulesetId != worldConfig.rulesetId) {
            logError("WorldRulesValidation",
                     "WorldRules rulesetId '" + rules.rulesetId +
                     "' does not match worldConfig.rulesetId '" + worldConfig.rulesetId + "'");
            ok = false;
        }

        auto checkNonNegative = [&](float value, const std::string& name) {
            if (value < 0.0f) {
                logError("WorldRulesValidation",
                         "Negative multiplier in WorldRules for " + name + ": " + std::to_string(value));
                ok = false;
            }
        };

        // XP
        checkNonNegative(rules.xp.baseRate, "xp.base_rate");
        checkNonNegative(rules.xp.groupBonusPerMember, "xp.group_bonus_per_member");
        checkNonNegative(rules.xp.hotZoneMultiplierDefault, "xp.hot_zone_multiplier_default");

        // Loot
        checkNonNegative(rules.loot.dropRateMultiplier, "loot.drop_rate_multiplier");
        checkNonNegative(rules.loot.coinRateMultiplier, "loot.coin_rate_multiplier");
        checkNonNegative(rules.loot.rareDropMultiplier, "loot.rare_drop_multiplier");

        // Death
        checkNonNegative(rules.death.xpLossMultiplier, "death.xp_loss_multiplier");

        // Hot zones
        for (const auto& hz : rules.hotZones) {
            if (hz.zoneId == 0) {
                logError("WorldRulesValidation", "Hot zone has invalid zone_id=0");
                ok = false;
            }
            checkNonNegative(hz.xpMultiplier, "hot_zone.xp_multiplier");
            checkNonNegative(hz.lootMultiplier, "hot_zone.loot_multiplier");
        }
    } catch (const std::exception& e) {
        logError("WorldRulesValidation",
                 std::string{"WorldRules validation failed: "} + e.what());
        ok = false;
    }

    return ok;
}

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

} // anonymous namespace

ValidationResult runAllValidations(const std::string& configRoot,
                                   const std::string& accountsRoot,
                                   const std::string& charactersRoot) {
    ValidationResult result;

    logInfo("Validator", "Starting REQ data validation...");
    logInfo("Validator", "  configRoot    = " + configRoot);
    logInfo("Validator", "  accountsRoot  = " + accountsRoot);
    logInfo("Validator", "  charactersRoot= " + charactersRoot);

    req::shared::WorldConfig worldConfig{};
    std::vector<req::shared::ZoneConfig> zoneConfigs;

    auto accumulate = [&](bool ok, const std::string& label) {
        if (!ok) {
            result.success = false;
            ++result.errorCount;
            logError("Validator", "Validation failed for: " + label);
        } else {
            logInfo("Validator", "Validation passed for: " + label);
        }
    };

    accumulate(validateConfigs(configRoot, worldConfig, zoneConfigs), "Config files");
    accumulate(validateNpcData(configRoot + "/zones"), "NPC data");
    accumulate(validateWorldRules(configRoot, worldConfig), "World rules");
    accumulate(validateAccountsAndCharacters(accountsRoot, charactersRoot, worldConfig, zoneConfigs),
               "Accounts & characters");

    if (result.success) {
        logInfo("Validator", "All validation checks passed.");
    } else {
        logError("Validator", "Validation finished with "
            + std::to_string(result.errorCount) + " failing pass(es).");
    }

    return result;
}

} // namespace req::datavalidator
