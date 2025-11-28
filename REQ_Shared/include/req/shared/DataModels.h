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
    std::uint32_t level { 1 };
    std::uint32_t homeWorldId { 0 };
    std::uint32_t lastWorldId { 0 };
    std::uint32_t lastZoneId { 0 };
    float positionX { 0.0f };
    float positionY { 0.0f };
    float positionZ { 0.0f };
    float heading { 0.0f };
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
    j["level"] = src.level;
    j["home_world_id"] = src.homeWorldId;
    j["last_world_id"] = src.lastWorldId;
    j["last_zone_id"] = src.lastZoneId;
    j["position_x"] = src.positionX;
    j["position_y"] = src.positionY;
    j["position_z"] = src.positionZ;
    j["heading"] = src.heading;
    j["inventory_slots"] = src.inventorySlots;
}

inline void from_json(const nlohmann::json& j, Character& dst) {
    dst.characterId = j.value("character_id", static_cast<std::uint64_t>(0));
    dst.accountId = j.value("account_id", static_cast<std::uint64_t>(0));
    dst.name = j.value("name", std::string{});
    dst.race = j.value("race", std::string{});
    dst.characterClass = j.value("class", std::string{});
    dst.level = j.value("level", static_cast<std::uint32_t>(1));
    dst.homeWorldId = j.value("home_world_id", static_cast<std::uint32_t>(0));
    dst.lastWorldId = j.value("last_world_id", static_cast<std::uint32_t>(0));
    dst.lastZoneId = j.value("last_zone_id", static_cast<std::uint32_t>(0));
    dst.positionX = j.value("position_x", 0.0f);
    dst.positionY = j.value("position_y", 0.0f);
    dst.positionZ = j.value("position_z", 0.0f);
    dst.heading = j.value("heading", 0.0f);
    
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
