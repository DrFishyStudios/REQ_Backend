#pragma once

#include <string>
#include <cstdint>
#include <vector>

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

struct NpcTemplate {
    std::uint32_t id { 0 };
    std::string name;
    std::uint32_t level { 1 };
    std::uint32_t factionId { 0 };
    std::uint32_t lootTableId { 0 };
    // TODO: Add stats, behaviors, abilities
};

} // namespace req::shared::data
