#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>

/**
 * NpcSpawnData.h
 * 
 * Lightweight data structures for NPC templates and spawn points.
 * These map directly to the JSON file formats defined in docs/NPC_DATA_FORMAT.md
 * 
 * Design Philosophy:
 * - Keep structures simple and close to JSON schema
 * - Use existing DataModels.h types for runtime NPC instances
 * - Avoid duplicating NpcTemplate/SpawnPoint from DataModels.h
 * - These are loading/configuration structures, not runtime state
 */

namespace req::zone {

/**
 * NpcTemplateData
 * 
 * Data loaded from npc_templates.json for a single NPC archetype.
 * This is a lightweight configuration structure that will be used to
 * create runtime ZoneNpc instances.
 * 
 * Maps to JSON schema in docs/NPC_DATA_FORMAT.md section 1.
 */
struct NpcTemplateData {
    std::int32_t npcId{ 0 };               // Unique template ID
    std::string name;                       // Display name (e.g., "A Decaying Skeleton")
    std::int32_t level{ 1 };                // NPC level
    std::string archetype;                  // Behavior archetype (e.g., "melee_trash")
    
    // Combat stats
    std::int32_t hp{ 100 };
    std::int32_t ac{ 10 };
    std::int32_t minDamage{ 1 };
    std::int32_t maxDamage{ 5 };
    
    // References
    std::int32_t factionId{ 0 };
    std::int32_t lootTableId{ 0 };
    std::string visualId;                   // Client-side model reference (int or string)
    
    // Behavior flags
    bool isSocial{ false };                 // Assists nearby NPCs
    bool canFlee{ false };                  // Flees at low HP
    bool isRoamer{ false };                 // Wanders from spawn point
    
    // AI parameters
    float aggroRadius{ 10.0f };             // Proximity aggro range
    float assistRadius{ 15.0f };            // Social aggro range
};

/**
 * NpcSpawnPointData
 * 
 * Data loaded from npc_spawns_<zone>.json for a single spawn location.
 * References an NPC template by ID and defines position/timing.
 * 
 * Maps to JSON schema in docs/NPC_DATA_FORMAT.md section 2.
 */
struct NpcSpawnPointData {
    std::int32_t spawnId{ 0 };              // Unique spawn point ID (per zone)
    std::int32_t npcId{ 0 };                // NPC template ID reference
    
    // Position
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    float heading{ 0.0f };                   // Facing direction in degrees
    
    // Respawn timing
    std::int32_t respawnSeconds{ 120 };
    std::int32_t respawnVarianceSeconds{ 0 };
    
    // Optional grouping
    std::string spawnGroup;                  // Logical camp/group identifier
};

/**
 * Position3D
 * 
 * Simple 3D position helper for spawn point positions.
 */
struct Position3D {
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };
};

/**
 * NpcDataRepository
 * 
 * Manages loading and lookup of NPC templates and spawn points.
 * Loads data from JSON files at zone startup.
 * 
 * Usage:
 *   NpcDataRepository repo;
 *   if (!repo.LoadNpcTemplates("config/npc_templates.json")) {
 *       // Handle error
 *   }
 *   if (!repo.LoadZoneSpawns("config/zones/npc_spawns_10.json")) {
 *       // Handle error
 *   }
 *   
 *   const NpcTemplateData* tmpl = repo.GetTemplate(1001);
 *   const std::vector<NpcSpawnPointData>& spawns = repo.GetZoneSpawns();
 */
class NpcDataRepository {
public:
    NpcDataRepository() = default;
    ~NpcDataRepository() = default;
    
    /**
     * Load NPC templates from JSON file.
     * 
     * @param path Path to npc_templates.json
     * @return true on success, false on error
     * 
     * Logs:
     * - INFO: File path, number of templates loaded
     * - WARN: Duplicate IDs, invalid data
     * - ERROR: File not found, parse errors
     */
    bool LoadNpcTemplates(const std::string& path);
    
    /**
     * Load zone spawn points from JSON file.
     * 
     * @param path Path to npc_spawns_<zone>.json
     * @return true on success, false on error
     * 
     * Logs:
     * - INFO: File path, number of spawns loaded, zone ID
     * - WARN: Invalid NPC ID references, duplicate spawn IDs
     * - ERROR: File not found, parse errors
     */
    bool LoadZoneSpawns(const std::string& path);
    
    /**
     * Get NPC template by ID.
     * 
     * @param npcId NPC template ID
     * @return Pointer to template, or nullptr if not found
     */
    const NpcTemplateData* GetTemplate(std::int32_t npcId) const;
    
    /**
     * Get all zone spawn points.
     * 
     * @return Reference to spawn points vector
     */
    const std::vector<NpcSpawnPointData>& GetZoneSpawns() const;
    
    /**
     * Get spawn point by ID.
     * 
     * @param spawnId Spawn point ID
     * @return Pointer to spawn point, or nullptr if not found
     */
    const NpcSpawnPointData* GetSpawnPoint(std::int32_t spawnId) const;
    
    /**
     * Get all NPC templates.
     * 
     * @return Map of template ID -> template data
     */
    const std::unordered_map<std::int32_t, NpcTemplateData>& GetAllTemplates() const;
    
    /**
     * Get number of loaded templates.
     */
    std::size_t GetTemplateCount() const;
    
    /**
     * Get number of loaded spawn points.
     */
    std::size_t GetSpawnCount() const;
    
    /**
     * Clear all loaded data.
     */
    void Clear();

private:
    std::unordered_map<std::int32_t, NpcTemplateData> templates_;
    std::vector<NpcSpawnPointData> spawnPoints_;
    std::uint32_t zoneId_{ 0 };  // Zone ID from spawn file
};

} // namespace req::zone
