#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/CharacterStore.h"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

/*
 * Character JSON Upgrade Utility
 * 
 * This utility loads all existing character JSON files and re-saves them,
 * upgrading them to the new schema with MMO stats.
 * 
 * Old character files missing new fields will get sensible defaults:
 * - XP: 0
 * - HP/Mana: Calculated based on race/class
 * - Stats: Calculated based on race/class
 * - Bind point: Set to current last known location
 * 
 * Usage:
 *   REQ_CharacterUpgrade.exe [characters_directory]
 * 
 * Default directory: data/characters
 */

int main(int argc, char* argv[]) {
    req::shared::initLogger("REQ_CharacterUpgrade");
    
    std::string charactersDir = "data/characters";
    
    if (argc > 1) {
        charactersDir = argv[1];
    }
    
    std::cout << "=== REQ Character JSON Upgrade Utility ===\n";
    std::cout << "Characters directory: " << charactersDir << "\n\n";
    
    if (!fs::exists(charactersDir)) {
        std::cout << "Directory does not exist: " << charactersDir << "\n";
        std::cout << "Nothing to upgrade.\n";
        return 0;
    }
    
    req::shared::CharacterStore store(charactersDir);
    
    int upgradeCount = 0;
    int skipCount = 0;
    int errorCount = 0;
    
    try {
        for (const auto& entry : fs::directory_iterator(charactersDir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }
            
            try {
                std::uint64_t charId = std::stoull(entry.path().stem().string());
                
                std::cout << "Processing character ID " << charId << "... ";
                
                // Load character (will apply defaults for missing fields)
                auto character = store.loadById(charId);
                if (!character.has_value()) {
                    std::cout << "SKIP (failed to load)\n";
                    skipCount++;
                    continue;
                }
                
                // Re-save character (writes full schema)
                if (!store.saveCharacter(*character)) {
                    std::cout << "ERROR (failed to save)\n";
                    errorCount++;
                    continue;
                }
                
                std::cout << "OK (upgraded: " << character->name << ", " 
                         << character->race << " " << character->characterClass 
                         << " level " << character->level << ")\n";
                upgradeCount++;
                
            } catch (const std::exception& e) {
                std::cout << "ERROR (" << e.what() << ")\n";
                errorCount++;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cout << "\nFilesystem error: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "Upgraded: " << upgradeCount << "\n";
    std::cout << "Skipped:  " << skipCount << "\n";
    std::cout << "Errors:   " << errorCount << "\n";
    
    if (upgradeCount > 0) {
        std::cout << "\nAll character JSON files have been upgraded to the new schema.\n";
        std::cout << "You can now start the servers with enhanced character data.\n";
    }
    
    return 0;
}
