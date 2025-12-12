#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <vector>

#include <boost/asio.hpp>

#include "../../../../REQ_Shared/include/req/shared/Types.h"
#include "../../../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../../../REQ_Shared/include/req/shared/MessageTypes.h"

namespace req::clientcore {

// ============================================================================
// Configuration
// ============================================================================

struct ClientConfig {
    std::string clientVersion{ "REQ-ClientCore-1.0" };
    std::string loginServerHost{ "127.0.0.1" };
    std::uint16_t loginServerPort{ 7777 };
};

// ============================================================================
// Session State (Opaque Handle)
// ============================================================================

/**
 * ClientSession
 * 
 * Tracks current session state across Login ? World ? Zone handshake.
 * This is an opaque data structure - clients should not modify fields directly.
 * All operations on the session are performed via free functions in req::clientcore.
 */
struct ClientSession {
    // Login state
    req::shared::SessionToken sessionToken{ 0 };
    std::uint64_t accountId{ 0 };
    bool isAdmin{ false };
    
    // World state
    req::shared::WorldId worldId{ 0 };
    std::string worldHost;
    std::uint16_t worldPort{ 0 };
    
    // Zone state
    req::shared::HandoffToken handoffToken{ 0 };
    req::shared::ZoneId zoneId{ 0 };
    std::string zoneHost;
    std::uint16_t zonePort{ 0 };
    std::uint64_t selectedCharacterId{ 0 };
    
    // Persistent zone connection (managed by connectToZone/disconnectFromZone)
    std::shared_ptr<boost::asio::io_context> zoneIoContext;
    std::shared_ptr<boost::asio::ip::tcp::socket> zoneSocket;
};

// ============================================================================
// Login Handshake
// ============================================================================

enum class LoginResult {
    Success,
    ConnectionFailed,
    InvalidCredentials,
    AccountBanned,
    NoWorldsAvailable,
    ProtocolError
};

struct LoginResponse {
    LoginResult result;
    std::string errorMessage;  // Human-readable error (if failed)
    std::vector<req::shared::protocol::WorldListEntry> availableWorlds;  // If success
};

/**
 * login
 * 
 * Connects to LoginServer and authenticates with username/password.
 * On success, populates session with sessionToken, worldId, worldHost/Port.
 * 
 * @param config Client configuration (server address, client version)
 * @param username Account username
 * @param password Account password
 * @param mode LoginMode::Login or LoginMode::Register
 * @param session [out] Session to populate with login state
 * @return LoginResponse with result and available worlds
 * 
 * Note: This is a blocking/synchronous call. Use in loading screens.
 */
LoginResponse login(
    const ClientConfig& config,
    const std::string& username,
    const std::string& password,
    req::shared::protocol::LoginMode mode,
    ClientSession& session);

// ============================================================================
// World Handshake
// ============================================================================

enum class CharacterListResult {
    Success,
    ConnectionFailed,
    InvalidSession,
    ProtocolError
};

struct CharacterListResponse {
    CharacterListResult result;
    std::string errorMessage;
    std::vector<req::shared::protocol::CharacterListEntry> characters;
};

/**
 * getCharacterList
 * 
 * Retrieves the list of characters for the current session.
 * Requires valid session.sessionToken and session.worldId from login().
 * 
 * @param session Current session (must have valid sessionToken, worldId)
 * @return CharacterListResponse with character list
 * 
 * Note: This is a blocking/synchronous call.
 */
CharacterListResponse getCharacterList(const ClientSession& session);

struct CharacterCreateResponse {
    CharacterListResult result;  // Same error codes as getCharacterList
    std::string errorMessage;
    req::shared::protocol::CharacterListEntry newCharacter;  // If success
};

/**
 * createCharacter
 * 
 * Creates a new character for the current session.
 * Requires valid session.sessionToken and session.worldId.
 * 
 * @param session Current session
 * @param name Character name
 * @param race Character race (e.g., "Human", "Elf")
 * @param characterClass Character class (e.g., "Warrior", "Wizard")
 * @return CharacterCreateResponse with new character details
 * 
 * Note: This is a blocking/synchronous call.
 */
CharacterCreateResponse createCharacter(
    const ClientSession& session,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass);

enum class EnterWorldResult {
    Success,
    ConnectionFailed,
    InvalidSession,
    InvalidCharacter,
    ProtocolError
};

struct EnterWorldResponse {
    EnterWorldResult result;
    std::string errorMessage;
};

/**
 * enterWorld
 * 
 * Requests to enter the world with a selected character.
 * On success, populates session with handoffToken, zoneId, zoneHost/Port.
 * 
 * @param session [in/out] Current session, updated with zone handoff info
 * @param characterId ID of character to enter world with
 * @return EnterWorldResponse with result
 * 
 * Note: This is a blocking/synchronous call.
 */
EnterWorldResponse enterWorld(
    ClientSession& session,
    std::uint64_t characterId);

// ============================================================================
// Zone Handshake
// ============================================================================

enum class ZoneAuthResult {
    Success,
    ConnectionFailed,
    InvalidHandoff,
    HandoffExpired,
    WrongZone,
    ProtocolError
};

struct ZoneAuthResponse {
    ZoneAuthResult result;
    std::string errorMessage;
    std::string welcomeMessage;  // If success
};

/**
 * connectToZone
 * 
 * Connects to ZoneServer and completes zone authentication.
 * Establishes a persistent connection stored in session.zoneSocket.
 * 
 * @param session [in/out] Current session, updated with zone connection
 * @return ZoneAuthResponse with result and welcome message
 * 
 * Note: This is a blocking call for the initial connection.
 *       After this returns Success, use non-blocking send/receive functions.
 */
ZoneAuthResponse connectToZone(ClientSession& session);

// ============================================================================
// Zone Communication (Non-blocking)
// ============================================================================

/**
 * sendMovementIntent
 * 
 * Sends a MovementIntent message to the zone server.
 * Non-blocking - returns immediately.
 * 
 * @param session Current session (must have active zone connection)
 * @param inputX Movement input X axis (-1.0 to 1.0)
 * @param inputY Movement input Y axis (-1.0 to 1.0)
 * @param facingYaw Facing direction in degrees (0-360)
 * @param jump Jump button state
 * @param sequenceNumber Client sequence number (increment per intent)
 * @return true if message was sent successfully, false otherwise
 */
bool sendMovementIntent(
    const ClientSession& session,
    float inputX, float inputY,
    float facingYaw, bool jump,
    std::uint32_t sequenceNumber);

/**
 * sendAttackRequest
 * 
 * Sends an AttackRequest message to the zone server.
 * Non-blocking - returns immediately.
 * 
 * @param session Current session
 * @param targetId Target entity ID (NPC or player)
 * @param abilityId Ability ID (0 = basic attack)
 * @param isBasicAttack true for basic attack, false for ability
 * @return true if message was sent successfully, false otherwise
 */
bool sendAttackRequest(
    const ClientSession& session,
    std::uint64_t targetId,
    std::uint32_t abilityId = 0,
    bool isBasicAttack = true);

/**
 * sendDevCommand
 * 
 * Sends a DevCommand message to the zone server (admin only).
 * Non-blocking - returns immediately.
 * 
 * @param session Current session (must have isAdmin = true)
 * @param command Command name (e.g., "suicide", "givexp", "setlevel")
 * @param param1 First parameter (optional)
 * @param param2 Second parameter (optional)
 * @return true if message was sent successfully, false otherwise
 */
bool sendDevCommand(
    const ClientSession& session,
    const std::string& command,
    const std::string& param1 = "",
    const std::string& param2 = "");

/**
 * ZoneMessage
 * 
 * Represents a message received from the zone server.
 * Contains raw message type and unparsed payload.
 * Use helper functions to parse specific message types.
 */
struct ZoneMessage {
    req::shared::MessageType type;
    std::string payload;  // Unparsed payload (client parses based on type)
};

/**
 * tryReceiveZoneMessage
 * 
 * Non-blocking receive: Attempts to read a message from the zone server.
 * Returns false if no messages are available (would block).
 * Returns true and fills outMessage if a message was received.
 * 
 * @param session Current session (must have active zone connection)
 * @param outMessage [out] Received message (if return is true)
 * @return true if message received, false if no messages available
 * 
 * Usage: Call this in your main loop to poll for zone messages.
 */
bool tryReceiveZoneMessage(
    const ClientSession& session,
    ZoneMessage& outMessage);

// ============================================================================
// Helper: Parse Common Zone Messages
// ============================================================================

/**
 * parsePlayerStateSnapshot
 * 
 * Helper to parse PlayerStateSnapshot from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed snapshot data
 * @return true if parse succeeded, false otherwise
 */
bool parsePlayerStateSnapshot(
    const std::string& payload,
    req::shared::protocol::PlayerStateSnapshotData& outData);

/**
 * parseAttackResult
 * 
 * Helper to parse AttackResult from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed attack result data
 * @return true if parse succeeded, false otherwise
 */
bool parseAttackResult(
    const std::string& payload,
    req::shared::protocol::AttackResultData& outData);

/**
 * parseDevCommandResponse
 * 
 * Helper to parse DevCommandResponse from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed response data
 * @return true if parse succeeded, false otherwise
 */
bool parseDevCommandResponse(
    const std::string& payload,
    req::shared::protocol::DevCommandResponseData& outData);

/**
 * parseEntitySpawn
 * 
 * Helper to parse EntitySpawn from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed entity spawn data
 * @return true if parse succeeded, false otherwise
 */
bool parseEntitySpawn(
    const std::string& payload,
    req::shared::protocol::EntitySpawnData& outData);

/**
 * parseEntityUpdate
 * 
 * Helper to parse EntityUpdate from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed entity update data
 * @return true if parse succeeded, false otherwise
 */
bool parseEntityUpdate(
    const std::string& payload,
    req::shared::protocol::EntityUpdateData& outData);

/**
 * parseEntityDespawn
 * 
 * Helper to parse EntityDespawn from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed entity despawn data
 * @return true if parse succeeded, false otherwise
 */
bool parseEntityDespawn(
    const std::string& payload,
    req::shared::protocol::EntityDespawnData& outData);

// ============================================================================
// Disconnect / Cleanup
// ============================================================================

/**
 * disconnectFromZone
 * 
 * Gracefully closes the zone connection.
 * Safe to call even if not connected.
 * 
 * @param session [in/out] Session with zone connection to close
 */
void disconnectFromZone(ClientSession& session);

} // namespace req::clientcore
