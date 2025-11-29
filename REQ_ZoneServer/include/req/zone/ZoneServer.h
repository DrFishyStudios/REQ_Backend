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
    
    // Simple flags
    bool isInitialized{ false };
    bool isDirty{ false };  // Position changed since last save
    bool combatStatsDirty{ false };  // Combat stats changed (HP/mana)
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
    
    // Combat
    void processAttack(ZonePlayer& attacker, req::shared::data::ZoneNpc& target, 
                      const req::shared::protocol::AttackRequestData& request);
    void broadcastAttackResult(const req::shared::protocol::AttackResultData& result);
    
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
    ZoneConfig zoneConfig_{};
    
    // Character persistence
    req::shared::CharacterStore characterStore_;
    
    // Zone simulation state
    boost::asio::steady_timer tickTimer_;
    boost::asio::steady_timer autosaveTimer_;
    std::uint64_t snapshotCounter_{ 0 };
    std::unordered_map<std::uint64_t, ZonePlayer> players_;
    std::unordered_map<ConnectionPtr, std::uint64_t> connectionToCharacterId_;
    
    // NPCs in this zone
    std::unordered_map<std::uint64_t, req::shared::data::ZoneNpc> npcs_;
};

} // namespace req::zone
