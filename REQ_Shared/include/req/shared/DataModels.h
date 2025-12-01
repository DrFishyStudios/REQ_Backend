#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>

#include "Types.h"

// Include json for serialization function definitions
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

namespace req::shared::data {

// Account data model (global)
struct Account {
    std::uint64_t accountId { 0 };
    std::string username;
    std::string passwordHash;
    bool isBanned { false };
    bool isAdmin { false };
    std::string displayName;
    std::string email;
};

// Character data model (global, associated with account and home world)
struct Character {
    std::uint64_t characterId { 0 };
    std::uint64_t accountId { 0 };
    std::string name;
    std::string race;
    std::string characterClass;
    
    // Progression
    std::uint32_t level { 1 };
    std::uint64_t xp { 0 };
    
    // Vitals
    std::int32_t hp { 100 };
    std::int32_t maxHp { 100 };
    std::int32_t mana { 100 };
    std::int32_t maxMana { 100 };
    
    // Primary Stats (EQ-classic style)
    std::int32_t strength { 75 };
    std::int32_t stamina { 75 };
    std::int32_t agility { 75 };
    std::int32_t dexterity { 75 };
    std::int32_t intelligence { 75 };
    std::int32_t wisdom { 75 };
    std::int32_t charisma { 75 };
    
    // World and Zone tracking
    std::uint32_t homeWorldId { 0 };
    std::uint32_t lastWorldId { 0 };
    std::uint32_t lastZoneId { 0 };
    
    // Last known position
    float positionX { 0.0f };
    float positionY { 0.0f };
    float positionZ { 0.0f };
    float heading { 0.0f };  // Facing direction in degrees (0-360)
    
    // Bind point (respawn location)
    std::int32_t bindWorldId { -1 };
    std::int32_t bindZoneId { -1 };
    float bindX { 0.0f };
    float bindY { 0.0f };
    float bindZ { 0.0f };
    
    // Inventory (placeholder)
    std::vector<std::string> inventorySlots;
};

// JSON serialization functions (inline for ADL)
inline void to_json(nlohmann::json& j, const Account& src) {
    j["account_id"] = src.accountId;
    j["username"] = src.username;
    j["password_hash"] = src.passwordHash;
    j["is_banned"] = src.isBanned;
    j["is_admin"] = src.isAdmin;
    j["display_name"] = src.displayName;
    j["email"] = src.email;
}

inline void from_json(const nlohmann::json& j, Account& dst) {
    dst.accountId = j.value("account_id", static_cast<std::uint64_t>(0));
    dst.username = j.value("username", std::string{});
    dst.passwordHash = j.value("password_hash", std::string{});
    dst.isBanned = j.value("is_banned", false);
    dst.isAdmin = j.value("is_admin", false);
    dst.displayName = j.value("display_name", std::string{});
    dst.email = j.value("email", std::string{});
}

inline void to_json(nlohmann::json& j, const Character& src) {
    j["character_id"] = src.characterId;
    j["account_id"] = src.accountId;
    j["name"] = src.name;
    j["race"] = src.race;
    j["class"] = src.characterClass;
    
    // Progression
    j["level"] = src.level;
    j["xp"] = src.xp;
    
    // Vitals
    j["hp"] = src.hp;
    j["max_hp"] = src.maxHp;
    j["mana"] = src.mana;
    j["max_mana"] = src.maxMana;
    
    // Primary Stats
    j["strength"] = src.strength;
    j["stamina"] = src.stamina;
    j["agility"] = src.agility;
    j["dexterity"] = src.dexterity;
    j["intelligence"] = src.intelligence;
    j["wisdom"] = src.wisdom;
    j["charisma"] = src.charisma;
    
    // World and Zone tracking
    j["home_world_id"] = src.homeWorldId;
    j["last_world_id"] = src.lastWorldId;
    j["last_zone_id"] = src.lastZoneId;
    
    // Last known position
    j["position_x"] = src.positionX;
    j["position_y"] = src.positionY;
    j["position_z"] = src.positionZ;
    j["heading"] = src.heading;
    
    // Bind point
    j["bind_world_id"] = src.bindWorldId;
    j["bind_zone_id"] = src.bindZoneId;
    j["bind_x"] = src.bindX;
    j["bind_y"] = src.bindY;
    j["bind_z"] = src.bindZ;
    
    // Inventory
    j["inventory_slots"] = src.inventorySlots;
}

inline void from_json(const nlohmann::json& j, Character& dst) {
    dst.characterId = j.value("character_id", static_cast<std::uint64_t>(0));
    dst.accountId = j.value("account_id", static_cast<std::uint64_t>(0));
    dst.name = j.value("name", std::string{});
    dst.race = j.value("race", std::string{});
    dst.characterClass = j.value("class", std::string{});
    
    // Progression (backward compatible defaults)
    dst.level = j.value("level", static_cast<std::uint32_t>(1));
    dst.xp = j.value("xp", static_cast<std::uint64_t>(0));
    
    // Vitals (backward compatible defaults)
    dst.hp = j.value("hp", static_cast<std::int32_t>(100));
    dst.maxHp = j.value("max_hp", static_cast<std::int32_t>(100));
    dst.mana = j.value("mana", static_cast<std::int32_t>(100));
    dst.maxMana = j.value("max_mana", static_cast<std::int32_t>(100));
    
    // Primary Stats (backward compatible defaults - baseline stats)
    dst.strength = j.value("strength", static_cast<std::int32_t>(75));
    dst.stamina = j.value("stamina", static_cast<std::int32_t>(75));
    dst.agility = j.value("agility", static_cast<std::int32_t>(75));
    dst.dexterity = j.value("dexterity", static_cast<std::int32_t>(75));
    dst.intelligence = j.value("intelligence", static_cast<std::int32_t>(75));
    dst.wisdom = j.value("wisdom", static_cast<std::int32_t>(75));
    dst.charisma = j.value("charisma", static_cast<std::int32_t>(75));
    
    // World and Zone tracking
    dst.homeWorldId = j.value("home_world_id", static_cast<std::uint32_t>(0));
    dst.lastWorldId = j.value("last_world_id", static_cast<std::uint32_t>(0));
    dst.lastZoneId = j.value("last_zone_id", static_cast<std::uint32_t>(0));
    
    // Last known position
    dst.positionX = j.value("position_x", 0.0f);
    dst.positionY = j.value("position_y", 0.0f);
    dst.positionZ = j.value("position_z", 0.0f);
    dst.heading = j.value("heading", 0.0f);
    
    // Bind point (backward compatible defaults: -1 means not set)
    dst.bindWorldId = j.value("bind_world_id", static_cast<std::int32_t>(-1));
    dst.bindZoneId = j.value("bind_zone_id", static_cast<std::int32_t>(-1));
    dst.bindX = j.value("bind_x", 0.0f);
    dst.bindY = j.value("bind_y", 0.0f);
    dst.bindZ = j.value("bind_z", 0.0f);
    
    // Handle inventory slots array with defaults
    if (j.contains("inventory_slots") && j["inventory_slots"].is_array()) {
        dst.inventorySlots.clear();
        for (const auto& item : j["inventory_slots"]) {
            if (item.is_string()) {
                dst.inventorySlots.push_back(item.get<std::string>());
            }
        }
    } else {
        dst.inventorySlots.clear();
    }
}

struct PlayerCore {
    PlayerId id { InvalidPlayerId };
    AccountId accountId { InvalidAccountId };
    std::string name;
    std::uint32_t level { 1 };
    WorldId worldId { InvalidWorldId };
    ZoneId lastZoneId { InvalidZoneId };
};

struct ItemDef {
    std::uint32_t id { 0 };
    std::string name;
    std::uint32_t iconId { 0 };
    std::uint32_t rarity { 0 };
    std::uint32_t maxStack { 1 };
    // TODO: Add more item attributes later
};

// ============================================================================
// NPC Template System (2.2)
// ============================================================================

/**
 * NpcBehaviorFlags
 * 
 * Boolean flags controlling NPC behavior and capabilities.
 * Based on REQ_GDD_v09 section 28.4 (NPC Behavior Flags).
 */
struct NpcBehaviorFlags {
    bool isRoamer{ false };         // Wanders around spawn area
    bool isStatic{ true };          // Stays at spawn point
    bool isSocial{ false };         // Assists nearby NPCs of same faction
    bool usesRanged{ false };       // Has ranged attacks
    bool callsForHelp{ false };     // Alerts nearby NPCs when attacked
    bool canFlee{ false };          // Runs away at low health
    bool immuneMez{ false };        // Immune to mesmerize
    bool immuneCharm{ false };      // Immune to charm
    bool immuneFear{ false };       // Immune to fear
    bool leashToSpawn{ true };      // Returns to spawn when pulled too far
};

/**
 * NpcBehaviorParams
 * 
 * Numeric parameters controlling NPC AI behavior ranges and timings.
 * Based on REQ_GDD_v09 section 27 (Spawn System).
 */
struct NpcBehaviorParams {
    float aggroRadius{ 800.0f };        // Detection range for hostiles
    float socialRadius{ 600.0f };       // Range to assist other NPCs
    float fleeHealthPercent{ 0.0f };    // HP % at which to flee (0 = never)
    float leashRadius{ 2000.0f };       // Max distance from spawn before leash
    float leashTimeoutSec{ 10.0f };     // Time in combat before forced leash
    float maxChaseDistance{ 2500.0f };  // Absolute max chase distance
    float preferredRange{ 200.0f };     // Preferred combat distance (melee/ranged)
    float assistDelaySec{ 0.5f };       // Delay before assisting other NPCs
};

/**
 * NpcStatBlock
 * 
 * Base stats for an NPC template, with level range support.
 * Stats are per-level and will be scaled at spawn time.
 */
struct NpcStatBlock {
    int levelMin{ 1 };              // Minimum level for this template
    int levelMax{ 1 };              // Maximum level for this template
    int hp{ 100 };                  // Base HP at levelMin
    int mana{ 0 };                  // Base mana (0 for non-casters)
    int ac{ 10 };                   // Armor class
    int atk{ 10 };                  // Attack rating
    
    // Primary stats (EQ-style)
    int str{ 10 };
    int sta{ 10 };
    int dex{ 10 };
    int agi{ 10 };
    int intl{ 10 };                 // 'intl' to avoid C++ keyword collision
    int wis{ 10 };
    int cha{ 10 };
};

/**
 * NpcTemplate
 * 
 * Complete template definition for spawning NPCs.
 * Loaded from data/npcs.json at server startup.
 * Based on REQ_GDD_v09 sections 18-19, 27, 28.4.
 */
struct NpcTemplate {
    std::int32_t id{ 0 };                   // Unique template ID
    std::string name;                        // Display name (e.g. "A Decaying Skeleton")
    std::string archetype;                   // Archetype tag (e.g. "melee_trash", "caster_elite")
    
    NpcStatBlock stats;                      // Base stats and level range
    
    std::int32_t factionId{ 0 };            // Faction for friend/foe logic
    std::int32_t lootTableId{ 0 };          // Loot table to use on death
    
    NpcBehaviorFlags behaviorFlags;          // Boolean behavior flags
    NpcBehaviorParams behaviorParams;        // Numeric behavior parameters
    
    // Package IDs for future extensibility
    std::string visualId;                    // Visual/model identifier
    std::string abilityPackageId;            // Ability set identifier
    std::string navigationPackageId;         // Navigation behavior identifier
    std::string behaviorPackageId;           // High-level behavior package
};

/**
 * NpcTemplateStore
 * 
 * Container for all loaded NPC templates.
 * Keyed by template ID for fast lookup.
 */
struct NpcTemplateStore {
    std::unordered_map<std::int32_t, NpcTemplate> templates;
};

// ============================================================================
// Spawn System (2.1)
// ============================================================================

/**
 * SpawnGroupEntry
 * 
 * Single weighted entry in a spawn group.
 * Multiple entries allow random selection with configurable probabilities.
 */
struct SpawnGroupEntry {
    std::int32_t npcId{ 0 };        // NPC template ID to spawn
    int weight{ 1 };                 // Relative spawn weight (higher = more likely)
};

/**
 * SpawnGroup
 * 
 * Collection of NPCs that can spawn at a spawn point.
 * Weighted random selection allows for common/uncommon/rare spawns.
 */
struct SpawnGroup {
    std::int32_t spawnGroupId{ 0 };
    std::vector<SpawnGroupEntry> entries;
};

/**
 * SpawnPoint
 * 
 * Individual spawn location in a zone.
 * Can reference either a spawn group (for variety) or a direct NPC ID.
 * Based on REQ_GDD_v09 section 27 (Spawn System & Population).
 */
struct SpawnPoint {
    std::int32_t spawnId{ 0 };              // Unique spawn point ID (per zone)
    
    // Position
    float x{ 0.0f };
    float y{ 0.0f };
    float z{ 0.0f };
    float heading{ 0.0f };                   // Facing direction in degrees
    
    // Spawn selection (use ONE of these)
    std::int32_t spawnGroupId{ 0 };         // If non-zero, use spawn group
    std::int32_t directNpcId{ 0 };          // If non-zero and no group, use this NPC ID
    
    // Respawn parameters
    float respawnTimeSec{ 120.0f };         // Base respawn time
    float respawnVarianceSec{ 0.0f };       // Random variance (±)
    
    // Optional behaviors
    float roamRadius{ 0.0f };               // Roaming radius (0 = stationary)
    float namedChance{ 0.0f };              // Chance to spawn a named/rare (0-1)
    
    // Time-of-day restrictions (TODO: implement day/night cycle)
    bool dayOnly{ false };                   // Only spawns during day
    bool nightOnly{ false };                 // Only spawns during night
};

/**
 * SpawnTable
 * 
 * Complete spawn configuration for a zone.
 * Loaded from data/spawns_<zoneId>.json at zone startup.
 */
struct SpawnTable {
    std::int32_t zoneId{ 0 };
    std::vector<SpawnPoint> spawnPoints;
    std::unordered_map<std::int32_t, SpawnGroup> spawnGroups;
};

// ============================================================================
// Zone-specific Runtime Data
// ============================================================================

/**
 * Corpse
 * 
 * Represents a player corpse left after death.
 * Contains position, expiry time, and will eventually hold items.
 */
struct Corpse {
    std::uint64_t corpseId{ 0 };                  // Unique corpse instance ID
    std::uint64_t ownerCharacterId{ 0 };          // Character who died
    std::int32_t worldId{ 0 };                    // World where corpse exists
    std::int32_t zoneId{ 0 };                     // Zone where corpse exists
    
    // Position
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    
    // Timestamps (stored as Unix seconds for easy serialization)
    std::int64_t createdAtUnix{ 0 };              // When corpse was created
    std::int64_t expiresAtUnix{ 0 };              // When corpse will decay
    
    // Future: inventory/loot
    // std::vector<ItemInstance> items;
};

// JSON serialization for Corpse
inline void to_json(nlohmann::json& j, const Corpse& src) {
    j["corpse_id"] = src.corpseId;
    j["owner_character_id"] = src.ownerCharacterId;
    j["world_id"] = src.worldId;
    j["zone_id"] = src.zoneId;
    j["pos_x"] = src.posX;
    j["pos_y"] = src.posY;
    j["pos_z"] = src.posZ;
    j["created_at_unix"] = src.createdAtUnix;
    j["expires_at_unix"] = src.expiresAtUnix;
}

inline void from_json(const nlohmann::json& j, Corpse& dst) {
    dst.corpseId = j.value("corpse_id", static_cast<std::uint64_t>(0));
    dst.ownerCharacterId = j.value("owner_character_id", static_cast<std::uint64_t>(0));
    dst.worldId = j.value("world_id", static_cast<std::int32_t>(0));
    dst.zoneId = j.value("zone_id", static_cast<std::int32_t>(0));
    dst.posX = j.value("pos_x", 0.0f);
    dst.posY = j.value("pos_y", 0.0f);
    dst.posZ = j.value("pos_z", 0.0f);
    dst.createdAtUnix = j.value("created_at_unix", static_cast<std::int64_t>(0));
    dst.expiresAtUnix = j.value("expires_at_unix", static_cast<std::int64_t>(0));
}

/**
 * NPC AI State Machine (2.3)
 */
enum class NpcAiState {
    Idle,       // Standing at spawn, periodic proximity scanning
    Alert,      // Detected potential target, validating before engaging
    Engaged,    // Actively fighting, maintaining hate table
    Leashing,   // Returning to spawn after being pulled too far
    Fleeing,    // Running away from combat (low HP)
    Dead        // Waiting for respawn timer
};

/**
 * ZoneNpc
 * 
 * Runtime state for an NPC active in a zone.
 * Combines template data with current position, HP, AI state, and hate management.
 * Based on REQ_GDD_v09 NPC archetype and behavior system.
 */
struct ZoneNpc {
    std::uint64_t npcId{ 0 };              // Unique NPC instance ID
    std::string name;                       // Display name
    std::int32_t level{ 1 };                // Level
    std::int32_t templateId{ 0 };           // NPC template ID (for respawn)
    std::int32_t spawnId{ 0 };              // Spawn point ID (for respawn)
    std::int32_t factionId{ 0 };            // Faction ID for friend/foe logic
    
    // Combat state
    std::int32_t currentHp{ 100 };
    std::int32_t maxHp{ 100 };
    bool isAlive{ true };
    
    // Position and orientation
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    float facingDegrees{ 0.0f };
    
    // Combat parameters
    std::int32_t minDamage{ 1 };
    std::int32_t maxDamage{ 5 };
    
    // Spawn point (for leashing/reset)
    float spawnX{ 0.0f };
    float spawnY{ 0.0f };
    float spawnZ{ 0.0f };
    
    // Respawn mechanics
    float respawnTimeSec{ 120.0f };         // How long until respawn after death
    float respawnTimerSec{ 0.0f };          // Current respawn countdown
    bool pendingRespawn{ false };           // Waiting to respawn
    
    // Behavior flags and params (copied from template for runtime access)
    NpcBehaviorFlags behaviorFlags;
    NpcBehaviorParams behaviorParams;
    
    // AI state machine
    NpcAiState aiState{ NpcAiState::Idle };
    
    // Hate table (entityId -> hate amount)
    std::unordered_map<std::uint64_t, float> hateTable;
    std::uint64_t currentTargetId{ 0 };     // EntityId with highest hate
    
    // AI timers
    float aggroScanTimer{ 0.0f };           // Timer for proximity aggro scans (Idle state)
    float leashTimer{ 0.0f };               // Time spent in combat at distance
    float meleeAttackCooldown{ 1.5f };      // Seconds between melee attacks
    float meleeAttackTimer{ 0.0f };         // Current attack cooldown
    
    // Movement
    float moveSpeed{ 50.0f };               // Movement speed in units/sec
};

} // namespace req::shared::data
