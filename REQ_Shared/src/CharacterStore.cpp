#include "../include/req/shared/CharacterStore.h"
#include "../include/req/shared/Logger.h"
#include "../include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_set>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace req::shared {
    
namespace {
    // EQ-classic valid races
    const std::unordered_set<std::string> VALID_RACES = {
        "Human", "Barbarian", "Erudite", "WoodElf", "HighElf", "DarkElf",
        "HalfElf", "Dwarf", "Troll", "Ogre", "Halfling", "Gnome"
    };

    // EQ-classic valid classes
    const std::unordered_set<std::string> VALID_CLASSES = {
        "Warrior", "Cleric", "Paladin", "Ranger", "ShadowKnight", "Druid",
        "Monk", "Bard", "Rogue", "Shaman", "Necromancer", "Wizard",
        "Magician", "Enchanter"
    };

    // TODO: Starting zone ID - placeholder for East Freeport
    constexpr std::uint32_t DEFAULT_STARTING_ZONE_ID = 10;
}

CharacterStore::CharacterStore(const std::string& charactersRootDirectory)
    : m_charactersRootDirectory(charactersRootDirectory) {
    
    // Ensure the directory exists
    try {
        if (!fs::exists(m_charactersRootDirectory)) {
            fs::create_directories(m_charactersRootDirectory);
            logInfo("CharacterStore", "Created characters directory: " + m_charactersRootDirectory);
        }
    } catch (const fs::filesystem_error& e) {
        std::string msg = "Failed to create characters directory: " + std::string(e.what());
        logError("CharacterStore", msg);
        throw std::runtime_error(msg);
    }
}

std::optional<data::Character> CharacterStore::loadById(std::uint64_t characterId) const {
    std::string filename = m_charactersRootDirectory + "/" + std::to_string(characterId) + ".json";
    
    if (!fs::exists(filename)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            logError("CharacterStore", "Failed to open character file: " + filename);
            return std::nullopt;
        }
        
        json j;
        file >> j;
        
        data::Character character = j.get<data::Character>();
        
        return character;
    } catch (const json::exception& e) {
        logError("CharacterStore", "JSON parse error loading character " + 
                std::to_string(characterId) + ": " + e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        logError("CharacterStore", "Error loading character " + std::to_string(characterId) + ": " + e.what());
        return std::nullopt;
    }
}

std::vector<data::Character> CharacterStore::loadCharactersForAccountAndWorld(
    std::uint64_t accountId, 
    std::uint32_t worldId) const {
    
    std::vector<data::Character> result;
    
    try {
        // Naive linear scan of all character files
        // TODO: Optimize with indexing (e.g., account_id -> [character_ids] map)
        for (const auto& entry : fs::directory_iterator(m_charactersRootDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                try {
                    std::uint64_t charId = std::stoull(entry.path().stem().string());
                    auto character = loadById(charId);
                    
                    if (character && 
                        character->accountId == accountId && 
                        character->homeWorldId == worldId) {
                        result.push_back(*character);
                    }
                } catch (const std::exception&) {
                    // Ignore files with non-numeric names
                    continue;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("CharacterStore", "Filesystem error during loadCharactersForAccountAndWorld: " + 
                std::string(e.what()));
    }
    
    return result;
}

data::Character CharacterStore::createCharacterForAccount(
    std::uint64_t accountId,
    std::uint32_t homeWorldId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass) {
    
    // Validate name uniqueness
    if (nameExists(name)) {
        std::string msg = "Character creation failed: name '" + name + "' already exists";
        logWarn("CharacterStore", msg);
        throw std::runtime_error(msg);
    }
    
    // Validate race
    if (!isValidRace(race)) {
        std::string msg = "Character creation failed: invalid race '" + race + "'";
        logError("CharacterStore", msg);
        throw std::runtime_error(msg);
    }
    
    // Validate class
    if (!isValidClass(characterClass)) {
        std::string msg = "Character creation failed: invalid class '" + characterClass + "'";
        logError("CharacterStore", msg);
        throw std::runtime_error(msg);
    }
    
    // TODO: Implement full race/class combination restrictions per EQ classic rules
    // For now, we allow any valid race/class combination
    
    // Generate new character ID
    std::uint64_t newCharacterId = generateNewCharacterId();
    
    // Create new character with defaults
    data::Character character;
    character.characterId = newCharacterId;
    character.accountId = accountId;
    character.name = name;
    character.race = race;
    character.characterClass = characterClass;
    character.level = 1;
    character.homeWorldId = homeWorldId;
    character.lastWorldId = homeWorldId;
    character.lastZoneId = DEFAULT_STARTING_ZONE_ID; // TODO: Load from starting locations table
    character.positionX = 0.0f; // TODO: Load actual starting position
    character.positionY = 0.0f;
    character.positionZ = 0.0f;
    character.heading = 0.0f;
    character.inventorySlots.clear(); // Empty inventory
    
    // Save to disk
    if (!saveCharacter(character)) {
        std::string msg = "Failed to save newly created character: " + name;
        logError("CharacterStore", msg);
        throw std::runtime_error(msg);
    }
    
    logInfo("CharacterStore", "Created new character: id=" + std::to_string(character.characterId) + 
            ", accountId=" + std::to_string(accountId) + 
            ", name=" + name + 
            ", race=" + race + 
            ", class=" + characterClass + 
            ", homeWorldId=" + std::to_string(homeWorldId));
    
    return character;
}

bool CharacterStore::saveCharacter(const data::Character& character) const {
    std::string filename = m_charactersRootDirectory + "/" + 
                          std::to_string(character.characterId) + ".json";
    
    try {
        json j = character;
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            logError("CharacterStore", "Failed to open character file for writing: " + filename);
            return false;
        }
        
        file << j.dump(4); // Pretty print with 4-space indent
        file.close();
        
        return true;
    } catch (const json::exception& e) {
        logError("CharacterStore", "JSON serialization error saving character " + 
                std::to_string(character.characterId) + ": " + e.what());
        return false;
    } catch (const std::exception& e) {
        logError("CharacterStore", "Error saving character " + 
                std::to_string(character.characterId) + ": " + e.what());
        return false;
    }
}

std::uint64_t CharacterStore::generateNewCharacterId() const {
    std::uint64_t maxId = 0;
    
    try {
        for (const auto& entry : fs::directory_iterator(m_charactersRootDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                try {
                    std::uint64_t id = std::stoull(entry.path().stem().string());
                    if (id > maxId) {
                        maxId = id;
                    }
                } catch (const std::exception&) {
                    // Ignore files with non-numeric names
                    continue;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("CharacterStore", "Filesystem error during ID generation: " + std::string(e.what()));
    }
    
    return maxId + 1;
}

bool CharacterStore::nameExists(const std::string& name) const {
    try {
        // Naive linear scan of all character files
        // TODO: Optimize with an index (e.g., name -> character_id map)
        for (const auto& entry : fs::directory_iterator(m_charactersRootDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                try {
                    std::uint64_t charId = std::stoull(entry.path().stem().string());
                    auto character = loadById(charId);
                    
                    if (character && character->name == name) {
                        return true;
                    }
                } catch (const std::exception&) {
                    // Ignore files with non-numeric names
                    continue;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("CharacterStore", "Filesystem error during nameExists check: " + std::string(e.what()));
    }
    
    return false;
}

bool CharacterStore::isValidRace(const std::string& race) const {
    return VALID_RACES.find(race) != VALID_RACES.end();
}

bool CharacterStore::isValidClass(const std::string& characterClass) const {
    return VALID_CLASSES.find(characterClass) != VALID_CLASSES.end();
}

} // namespace req::shared
