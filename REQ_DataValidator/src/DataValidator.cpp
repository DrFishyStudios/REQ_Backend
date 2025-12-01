#include "../include/req/datavalidator/DataValidator.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <vector>
#include <string>

using req::shared::logInfo;
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

bool validateItemsAndLoot(const std::string& itemsRoot,
                          const std::string& lootRoot);

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
    accumulate(validateItemsAndLoot("data/items", "data/loot"), "Items & loot");

    if (result.success) {
        logInfo("Validator", "All validation checks passed.");
    } else {
        logError("Validator", "Validation finished with "
            + std::to_string(result.errorCount) + " failing pass(es).");
    }

    return result;
}

} // namespace req::datavalidator
