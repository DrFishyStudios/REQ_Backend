#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <unordered_map>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/Connection.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/CharacterStore.h"
#include "../../REQ_Shared/include/req/shared/AccountStore.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::zone {

/**
 * ZonePlayer
 * 
 * In-memory state for a player currently active in this zone.
 * Tracks position, velocity, last input, validation data, and combat state.
 */
struct ZonePlayer {
    std::uint64_t accountId{ 0 };          // Account owner
    std::uint64_t characterId{ 0 };
    
    // Admin flag (cached from Account on zone entry)
    bool isAdmin{ false };
    
    // Connection for sending messages
    std::shared_ptr<req::shared::net::Connection> connection;
    
    // Current state
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    float velX{ 0.0f };
    float velY{ 0.0f };
    float velZ{ 0.0f };
    float yawDegrees{ 0.0f };
    
    // Last valid position for snap-back (anti-cheat)
    float lastValidPosX{ 0.0f };
    float lastValidPosY{ 0.0f };
    float lastValidPosZ{ 0.0f };
    
    // Last input from client
    float inputX{ 0.0f };
    float inputY{ 0.0f };
    bool isJumpPressed{ false };
    std::uint32_t lastSequenceNumber{ 0 };
    
    // Combat state (loaded from character, persisted on zone exit)
    std::int32_t level{ 1 };
    std::uint64_t xp{ 0 };  // Character XP
    std::int32_t hp{ 100 };
    std::int32_t maxHp{ 100 };
    std::int32_t mana{ 100 };
    std::int32_t maxMana{ 100 };
    
    // Primary stats (EQ-classic style)
    std::int32_t strength{ 75 };
    std::int32_t stamina{ 75 };
    std::int32_t agility{ 75 };
    std::int32_t dexterity{ 75 };
    std::int32_t intelligence{ 75 };
    std::int32_t wisdom{ 75 };
    std::int32_t charisma{ 75 };
    
    // Death state
    bool isDead{ false };  // true if player is currently dead
    
    // Simple flags
    bool isInitialized{ false };
    bool isDirty{ false };  // Position changed since last save
    bool combatStatsDirty{ false };  // Combat stats changed (HP/mana/XP)
};

// Use shared ZoneConfig from Config.h (no duplicate definition needed)
using ZoneConfig = req::shared::ZoneConfig;

class ZoneServer {
public:
    ZoneServer(std::uint32_t worldId,
               std::uint32_t zoneId,
               const std::string& zoneName,
               const std::string& address,
               std::uint16_t port,
               const req::shared::WorldRules& worldRules,
               const req::shared::XpTable& xpTable,
               const std::string& charactersPath = "data/characters");

    void run();
    void stop();
    
    // Set zone configuration (safe spawn point, interest management, etc.)
    void setZoneConfig(const ZoneConfig& config);

private:
    using Tcp = boost::asio::ip::tcp;
    using ConnectionPtr = std::shared_ptr<req::shared::net::Connection>;

    void startAccept();
    void handleNewConnection(Tcp::socket socket);

    void handleMessage(const req::shared::MessageHeader& header,
                       const req::shared::net::Connection::ByteArray& payload,
                       ConnectionPtr connection);
    
    // Zone entry spawn logic
    void spawnPlayer(req::shared::data::Character& character, ZonePlayer& player);
    
    // Position persistence
    void savePlayerPosition(std::uint64_t characterId);
    void saveAllPlayerPositions();
    
    // NPC management
    void loadNpcsForZone();
    void updateNpc(req::shared::data::ZoneNpc& npc, float deltaSeconds);
    void updateNpcAi(req::shared::data::ZoneNpc& npc, float deltaSeconds);
    
    // Hate/Aggro system (Phase 2.3)
    void addHate(req::shared::data::ZoneNpc& npc, std::uint64_t entityId, float amount);
    std::uint64_t getTopHateTarget(const req::shared::data::ZoneNpc& npc) const;
    void clearHate(req::shared::data::ZoneNpc& npc);
    
    // Combat
    void processAttack(ZonePlayer& attacker, req::shared::data::ZoneNpc& target, 
                      const req::shared::protocol::AttackRequestData& request);
    void broadcastAttackResult(const req::shared::protocol::AttackResultData& result);
    
    // Death and respawn
    void handlePlayerDeath(ZonePlayer& player);
    void respawnPlayer(ZonePlayer& player);
    void processCorpseDecay();
    
    // Dev commands (for testing)
    void devGiveXp(std::uint64_t characterId, std::int64_t amount);
    void devSetLevel(std::uint64_t characterId, std::uint32_t level);
    void devSuicide(std::uint64_t characterId);
    void devDamageSelf(std::uint64_t characterId, std::int32_t amount);
    
    // Group operations (Phase 3)
    req::shared::data::Group& createGroup(std::uint64_t leaderCharacterId);
    bool addMemberToGroup(std::uint64_t groupId, std::uint64_t characterId);
    bool removeMemberFromGroup(std::uint64_t groupId, std::uint64_t characterId);
    void disbandGroup(std::uint64_t groupId);
    req::shared::data::Group* getGroupById(std::uint64_t groupId);
    req::shared::data::Group* getGroupForCharacter(std::uint64_t characterId);
    bool isCharacterInGroup(std::uint64_t characterId) const;
    bool isGroupFull(const req::shared::data::Group& group) const;
    
    void handleGroupInvite(std::uint64_t inviterCharId, const std::string& targetName);
    void handleGroupAccept(std::uint64_t targetCharId, std::uint64_t groupId);
    void handleGroupDecline(std::uint64_t targetCharId, std::uint64_t groupId);
    void handleGroupLeave(std::uint64_t characterId);
    void handleGroupKick(std::uint64_t leaderCharId, std::uint64_t targetCharId);
    void handleGroupDisband(std::uint64_t leaderCharId);
    std::optional<std::uint64_t> getCharacterGroup(std::uint64_t characterId) const;
    
    // Group XP sharing (Phase 3)
    void awardXpForNpcKill(req::shared::data::ZoneNpc& target, ZonePlayer& attacker);
    
    // Player disconnect handling
    void removePlayer(std::uint64_t characterId);
    void onConnectionClosed(ConnectionPtr connection);
    
    // Simulation tick
    void scheduleNextTick();
    void onTick(const boost::system::error_code& ec);
    void updateSimulation(float dt);
    void broadcastSnapshots();
    
    // Auto-save tick
    void scheduleAutosave();
    void onAutosave(const boost::system::error_code& ec);

    boost::asio::io_context  ioContext_{};
    Tcp::acceptor            acceptor_;
    std::vector<ConnectionPtr> connections_{};

    std::uint32_t            worldId_{};
    std::uint32_t            zoneId_{};
    std::string              zoneName_{};
    std::string              address_{};
    std::uint16_t            port_{};
    
    // Zone configuration
    ZoneConfig zoneConfig_;
    
    // World rules and XP tables
    req::shared::WorldRules worldRules_;
    req::shared::XpTable xpTable_;
    
    // NPC templates and spawn system (Phase 2)
    req::shared::data::NpcTemplateStore npcTemplates_;
    req::shared::data::SpawnTable spawnTable_;
    
    // Character persistence
    req::shared::CharacterStore characterStore_;
    req::shared::AccountStore accountStore_;
    
    // Zone simulation state
    boost::asio::steady_timer tickTimer_;
    boost::asio::steady_timer autosaveTimer_;
    std::uint64_t snapshotCounter_{ 0 };
    std::unordered_map<std::uint64_t, ZonePlayer> players_;
    std::unordered_map<ConnectionPtr, std::uint64_t> connectionToCharacterId_;
    
    // NPCs in this zone
    std::unordered_map<std::uint64_t, req::shared::data::ZoneNpc> npcs_;
    
    // Corpses in this zone
    std::unordered_map<std::uint64_t, req::shared::data::Corpse> corpses_;
    std::uint64_t nextCorpseId_{ 1 };
    
    // Groups in this zone (Phase 3)
    std::unordered_map<std::uint64_t, req::shared::data::Group> groups_;
    std::uint64_t nextGroupId_{ 1 };
    
    // Character to group lookup (for fast group membership checks)
    std::unordered_map<std::uint64_t, std::uint64_t> characterToGroupId_;
};

} // namespace req::zone
