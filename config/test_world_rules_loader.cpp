// Quick test to verify WorldRules loader
// Compile and run to test: just include this in a test project or comment out for now

#include "../REQ_Shared/include/req/shared/Config.h"
#include "../REQ_Shared/include/req/shared/Logger.h"
#include <iostream>

int main() {
    try {
        auto rules = req::shared::loadWorldRules("config/world_rules_classic_plus_qol.json");
        
        std::cout << "\n=== WorldRules Loaded Successfully ===\n";
        std::cout << "Ruleset ID: " << rules.rulesetId << "\n";
        std::cout << "Display Name: " << rules.displayName << "\n";
        std::cout << "Description: " << rules.description << "\n\n";
        
        std::cout << "XP Rules:\n";
        std::cout << "  Base Rate: " << rules.xp.baseRate << "\n";
        std::cout << "  Group Bonus Per Member: " << rules.xp.groupBonusPerMember << "\n";
        std::cout << "  Hot Zone Multiplier Default: " << rules.xp.hotZoneMultiplierDefault << "\n\n";
        
        std::cout << "Loot Rules:\n";
        std::cout << "  Drop Rate Multiplier: " << rules.loot.dropRateMultiplier << "\n";
        std::cout << "  Coin Rate Multiplier: " << rules.loot.coinRateMultiplier << "\n";
        std::cout << "  Rare Drop Multiplier: " << rules.loot.rareDropMultiplier << "\n\n";
        
        std::cout << "Death Rules:\n";
        std::cout << "  XP Loss Multiplier: " << rules.death.xpLossMultiplier << "\n";
        std::cout << "  Corpse Run Enabled: " << (rules.death.corpseRunEnabled ? "true" : "false") << "\n";
        std::cout << "  Corpse Decay Minutes: " << rules.death.corpseDecayMinutes << "\n\n";
        
        std::cout << "UI Helpers:\n";
        std::cout << "  Con Colors Enabled: " << (rules.uiHelpers.conColorsEnabled ? "true" : "false") << "\n";
        std::cout << "  Minimap Enabled: " << (rules.uiHelpers.minimapEnabled ? "true" : "false") << "\n";
        std::cout << "  Quest Tracker Enabled: " << (rules.uiHelpers.questTrackerEnabled ? "true" : "false") << "\n";
        std::cout << "  Corpse Arrow Enabled: " << (rules.uiHelpers.corpseArrowEnabled ? "true" : "false") << "\n\n";
        
        std::cout << "Hot Zones (" << rules.hotZones.size() << "):\n";
        for (const auto& hz : rules.hotZones) {
            std::cout << "  Zone ID: " << hz.zoneId << "\n";
            std::cout << "    XP Multiplier: " << hz.xpMultiplier << "\n";
            std::cout << "    Loot Multiplier: " << hz.lootMultiplier << "\n";
            std::cout << "    Start Date: " << (hz.startDate.empty() ? "<null>" : hz.startDate) << "\n";
            std::cout << "    End Date: " << (hz.endDate.empty() ? "<null>" : hz.endDate) << "\n";
        }
        
        std::cout << "\n? All fields loaded successfully!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
