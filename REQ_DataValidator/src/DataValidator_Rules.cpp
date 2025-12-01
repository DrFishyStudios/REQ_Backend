#include "../include/req/datavalidator/DataValidator.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <string>
#include <exception>

using req::shared::logInfo;
using req::shared::logWarn;
using req::shared::logError;

namespace req::datavalidator {

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

} // namespace req::datavalidator
