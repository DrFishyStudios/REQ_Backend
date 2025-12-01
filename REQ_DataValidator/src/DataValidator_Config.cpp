#include "../include/req/datavalidator/DataValidator.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <vector>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

using req::shared::logInfo;
using req::shared::logWarn;
using req::shared::logError;

namespace req::datavalidator {

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

} // namespace req::datavalidator
