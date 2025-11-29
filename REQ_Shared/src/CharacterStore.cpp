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
    
    // Helper: Calculate base HP for level 1 character
    std::int32_t calculateBaseHP(const std::string& race, const std::string& characterClass) {
        // Base HP formula: 100 + stamina_bonus + class_modifier
        // This is simplified - real EQ has complex formulas
        std::int32_t baseHP = 100;
        
        // Class HP modifiers
        if (characterClass == "Warrior") baseHP += 20;
        else if (characterClass == "Paladin" || characterClass == "ShadowKnight" || characterClass == "Ranger") baseHP += 15;
        else if (characterClass == "Monk" || characterClass == "Rogue" || characterClass == "Bard") baseHP += 10;
        else if (characterClass == "Cleric" || characterClass == "Druid" || characterClass == "Shaman") baseHP += 5;
        // Casters get base HP
        
        // Race HP bonuses
        if (race == "Barbarian" || race == "Troll" || race == "Ogre") baseHP += 10;
        else if (race == "Dwarf") baseHP += 5;
        else if (race == "Gnome" || race == "Halfling") baseHP -= 5;
        
        return baseHP;
    }
    
    // Helper: Calculate base Mana for level 1 character
    std::int32_t calculateBaseMana(const std::string& race, const std::string& characterClass) {
        // Non-casters have 0 mana
        if (characterClass == "Warrior" || characterClass == "Rogue" || characterClass == "Monk") {
            return 0;
        }
        
        // Base mana for casters
        std::int32_t baseMana = 100;
        
        // Class mana modifiers
        if (characterClass == "Wizard" || characterClass == "Magician" || characterClass == "Necromancer" || characterClass == "Enchanter") {
            baseMana += 20; // Pure casters
        } else if (characterClass == "Cleric" || characterClass == "Druid" || characterClass == "Shaman") {
            baseMana += 15; // Priests
        } else if (characterClass == "Paladin" || characterClass == "ShadowKnight" || characterClass == "Ranger" || characterClass == "Bard") {
            baseMana += 10; // Hybrids
        }
        
        // Race mana bonuses
        if (race == "Erudite" || race == "HighElf") baseMana += 10;
        else if (race == "DarkElf" || race == "Gnome") baseMana += 5;
        
        return baseMana;
    }
    
    // Helper: Get racial stat modifiers
    struct StatModifiers {
        std::int32_t str, sta, agi, dex, intel, wis, cha;
    };
    
    StatModifiers getRaceStatModifiers(const std::string& race) {
        // Baseline stats are 75 for all
        // These are modifiers applied on top of that
        // Based on EQ classic racial stat modifiers
        
        if (race == "Human") return {0, 0, 0, 0, 0, 0, 0}; // Average
        if (race == "Barbarian") return {10, 10, -5, 0, -5, -5, -5}; // Strong, tough, clumsy
        if (race == "Erudite") return {-10, -5, -5, -5, 10, 10, -5}; // Smart, wise, weak
        if (race == "WoodElf") return {-5, -5, 10, 0, 0, 5, 0}; // Agile, wise
        if (race == "HighElf") return {-10, -5, 10, 0, 10, 5, 5}; // Agile, intelligent, charismatic
        if (race == "DarkElf") return {-10, -5, 15, 10, 10, 0, 0}; // Very agile, dexterous, intelligent
        if (race == "HalfElf") return {0, -5, 10, 0, 0, 0, 5}; // Balanced with slight bonuses
        if (race == "Dwarf") return {10, 10, -5, 5, -5, 5, -10}; // Strong, tough, wise
        if (race == "Troll") return {15, 15, -5, -5, -10, -5, -15}; // Very strong, tough, dumb
        if (race == "Ogre") return {20, 15, -10, -10, -10, -5, -15}; // Extremely strong, very dumb
        if (race == "Halfling") return {-10, -5, 15, 15, -5, 0, 5}; // Agile, dexterous, charming
        if (race == "Gnome") return {-10, -5, 10, 10, 10, 0, 5}; // Agile, dexterous, intelligent
        
        return {0, 0, 0, 0, 0, 0, 0}; // Default
    }
    
    // Helper: Get class stat bonuses
    StatModifiers getClassStatModifiers(const std::string& characterClass) {
        // Classes get small bonuses to their primary stats
        
        if (characterClass == "Warrior") return {5, 5, 0, 0, 0, 0, 0};
        if (characterClass == "Cleric") return {0, 0, 0, 0, 0, 5, 5};
        if (characterClass == "Paladin") return {5, 5, 0, 0, 0, 5, 5};
        if (characterClass == "Ranger") return {5, 5, 5, 0, 0, 5, 0};
        if (characterClass == "ShadowKnight") return {5, 5, 0, 0, 5, 0, 0};
        if (characterClass == "Druid") return {0, 0, 0, 0, 0, 10, 0};
        if (characterClass == "Monk") return {5, 5, 10, 10, 0, 0, 0};
        if (characterClass == "Bard") return {0, 0, 5, 10, 0, 0, 10};
        if (characterClass == "Rogue") return {0, 0, 10, 15, 0, 0, 0};
        if (characterClass == "Shaman") return {0, 0, 0, 0, 0, 10, 5};
        if (characterClass == "Necromancer") return {0, 0, 0, 10, 10, 0, 0};
        if (characterClass == "Wizard") return {0, 0, 0, 0, 15, 0, 0};
        if (characterClass == "Magician") return {0, 0, 0, 0, 15, 0, 0};
        if (characterClass == "Enchanter") return {0, 0, 0, 0, 10, 0, 10};
        
        return {0, 0, 0, 0, 0, 0, 0};
    }
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

data::Character CharacterStore::createDefaultCharacter(
    std::uint64_t accountId,
    std::uint32_t homeWorldId,
    std::uint32_t homeZoneId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass,
    float startX,
    float startY,
    float startZ) {
    
    data::Character character;
    
    // Basic identity
    character.characterId = 0; // Caller must assign ID
    character.accountId = accountId;
    character.name = name;
    character.race = race;
    character.characterClass = characterClass;
    
    // Progression
    character.level = 1;
    character.xp = 0;
    
    // Calculate vitals based on race and class
    character.maxHp = calculateBaseHP(race, characterClass);
    character.hp = character.maxHp; // Start at full HP
    character.maxMana = calculateBaseMana(race, characterClass);
    character.mana = character.maxMana; // Start at full mana
    
    // Calculate stats based on race and class
    // Start with baseline 75 for all stats
    character.strength = 75;
    character.stamina = 75;
    character.agility = 75;
    character.dexterity = 75;
    character.intelligence = 75;
    character.wisdom = 75;
    character.charisma = 75;
    
    // Apply racial modifiers
    auto raceModifiers = getRaceStatModifiers(race);
    character.strength += raceModifiers.str;
    character.stamina += raceModifiers.sta;
    character.agility += raceModifiers.agi;
    character.dexterity += raceModifiers.dex;
    character.intelligence += raceModifiers.intel;
    character.wisdom += raceModifiers.wis;
    character.charisma += raceModifiers.cha;
    
    // Apply class modifiers
    auto classModifiers = getClassStatModifiers(characterClass);
    character.strength += classModifiers.str;
    character.stamina += classModifiers.sta;
    character.agility += classModifiers.agi;
    character.dexterity += classModifiers.dex;
    character.intelligence += classModifiers.intel;
    character.wisdom += classModifiers.wis;
    character.charisma += classModifiers.cha;
    
    // World and zone tracking
    character.homeWorldId = homeWorldId;
    character.lastWorldId = homeWorldId;
    character.lastZoneId = homeZoneId;
    
    // Starting position
    character.positionX = startX;
    character.positionY = startY;
    character.positionZ = startZ;
    character.heading = 0.0f; // Facing north
    
    // Bind point (set to starting location)
    character.bindWorldId = static_cast<std::int32_t>(homeWorldId);
    character.bindZoneId = static_cast<std::int32_t>(homeZoneId);
    character.bindX = startX;
    character.bindY = startY;
    character.bindZ = startZ;
    
    // Empty inventory
    character.inventorySlots.clear();
    
    return character;
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
    
    // TODO: Load starting positions from a starting locations table
    // For now, use default starting zone (East Freeport) and position
    std::uint32_t startingZoneId = DEFAULT_STARTING_ZONE_ID;
    float startX = 0.0f;
    float startY = 0.0f;
    float startZ = 0.0f;
    
    // Create character with default stats using helper
    data::Character character = createDefaultCharacter(
        accountId,
        homeWorldId,
        startingZoneId,
        name,
        race,
        characterClass,
        startX,
        startY,
        startZ
    );
    
    // Assign the generated ID
    character.characterId = newCharacterId;
    
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
            ", level=" + std::to_string(character.level) +
            ", hp=" + std::to_string(character.hp) + "/" + std::to_string(character.maxHp) +
            ", mana=" + std::to_string(character.mana) + "/" + std::to_string(character.maxMana) +
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
