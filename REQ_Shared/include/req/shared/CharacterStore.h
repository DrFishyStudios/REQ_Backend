#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include <vector>

#include "DataModels.h"

namespace req::shared {

/**
 * CharacterStore manages character persistence to disk using JSON files.
 * Each character is stored in: data/characters/<character_id>.json
 * 
 * This is a simple single-threaded implementation for prototyping.
 * Concurrency and advanced indexing will be added later.
 */
class CharacterStore {
public:
    explicit CharacterStore(const std::string& charactersRootDirectory);

    /**
     * Load a character by ID.
     * Returns std::nullopt if not found.
     */
    std::optional<data::Character> loadById(std::uint64_t characterId) const;

    /**
     * Load all characters for a given account and world.
     * Returns an empty vector if none found.
     * Note: This performs a linear scan of all character files (naive implementation).
     */
    std::vector<data::Character> loadCharactersForAccountAndWorld(
        std::uint64_t accountId, 
        std::uint32_t worldId) const;

    /**
     * Create a new character for an account.
     * 
     * Validates race and class against EQ-classic lists.
     * Ensures name uniqueness (naive scan for now).
     * Sets default starting position and zone.
     * 
     * TODO: Implement full race/class restrictions per EQ classic rules.
     * TODO: Load starting positions from a starting locations table.
     * 
     * Returns the newly created character.
     * Throws std::runtime_error if name already exists or validation fails.
     */
    data::Character createCharacterForAccount(
        std::uint64_t accountId,
        std::uint32_t homeWorldId,
        const std::string& name,
        const std::string& race,
        const std::string& characterClass);

    /**
     * Save a character to disk.
     * Returns true on success, false on failure.
     */
    bool saveCharacter(const data::Character& character) const;

private:
    std::string m_charactersRootDirectory;

    /**
     * Generate a new unique character ID by scanning existing files.
     * Simple implementation: finds max ID + 1.
     */
    std::uint64_t generateNewCharacterId() const;

    /**
     * Check if a character name already exists.
     * Note: Naive linear scan - will need optimization later.
     */
    bool nameExists(const std::string& name) const;

    /**
     * Validate race against EQ-classic list.
     */
    bool isValidRace(const std::string& race) const;

    /**
     * Validate class against EQ-classic list.
     */
    bool isValidClass(const std::string& characterClass) const;
};

} // namespace req::shared
