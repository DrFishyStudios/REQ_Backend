#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Types.h"

/*
 * ProtocolSchemas.h
 * 
 * Wire-level payload format definitions for REQ backend handshake protocol.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
 * This matches section 14.5 of the GDD.
 * 
 * Protocol Version: 1
 */

namespace req::shared::protocol {

// ============================================================================
// Enums
// ============================================================================

enum class LoginMode {
    Login,
    Register
};

// ============================================================================
// Data Structures for Parsed Payloads
// ============================================================================

struct WorldListEntry {
    WorldId worldId{ 0 };
    std::string worldName;
    std::string worldHost;
    std::uint16_t worldPort{ 0 };
    std::string rulesetId;
};

struct LoginResponseData {
    bool success{ false };
    
    // Success fields
    SessionToken sessionToken{ InvalidSessionToken };
    std::vector<WorldListEntry> worlds;
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};

struct WorldAuthResponseData {
    bool success{ false };
    
    // Success fields
    HandoffToken handoffToken{ InvalidHandoffToken };
    ZoneId zoneId{ 0 };
    std::string zoneHost;
    std::uint16_t zonePort{ 0 };
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};

struct ZoneAuthResponseData {
    bool success{ false };
    
    // Success fields
    std::string welcomeMessage;
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};

// Character list entry
struct CharacterListEntry {
    std::uint64_t characterId{ 0 };
    std::string name;
    std::string race;
    std::string characterClass;
    std::uint32_t level{ 1 };
};

struct CharacterListResponseData {
    bool success{ false };
    
    // Success fields
    std::vector<CharacterListEntry> characters;
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};

struct CharacterCreateResponseData {
    bool success{ false };
    
    // Success fields
    std::uint64_t characterId{ 0 };
    std::string name;
    std::string race;
    std::string characterClass;
    std::uint32_t level{ 1 };
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};

struct EnterWorldResponseData {
    bool success{ false };
    
    // Success fields
    HandoffToken handoffToken{ InvalidHandoffToken };
    ZoneId zoneId{ 0 };
    std::string zoneHost;
    std::uint16_t zonePort{ 0 };
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};

// ============================================================================
// LoginRequest / LoginResponse
// ============================================================================

/*
 * LoginRequest (client ? LoginServer)
 * 
 * Payload format: username|password|clientVersion|mode
 * 
 * Fields:
 *   - username: player account username
 *   - password: player account password (plaintext for now, TODO: hash)
 *   - clientVersion: client version string for compatibility checks
 *   - mode: "login" or "register" (defaults to "login" if omitted)
 * 
 * Example: "player1|mypassword|0.1.0|login"
 * Example: "newuser|newpass|0.1.0|register"
 */
std::string buildLoginRequestPayload(
    const std::string& username,
    const std::string& password,
    const std::string& clientVersion,
    LoginMode mode = LoginMode::Login);

bool parseLoginRequestPayload(
    const std::string& payload,
    std::string& outUsername,
    std::string& outPassword,
    std::string& outClientVersion,
    LoginMode& outMode);

/*
 * LoginResponse (LoginServer ? client)
 * 
 * Success format: OK|sessionToken|worldCount|world1Data|world2Data|...
 * 
 * World data format (comma-separated): worldId,worldName,worldHost,worldPort,rulesetId
 * 
 * Success fields:
 *   - status: "OK"
 *   - sessionToken: decimal session token
 *   - worldCount: number of available worlds (decimal)
 *   - worldData: comma-separated world entries (one per available world)
 * 
 * Example: "OK|123456789|2|1,MainWorld,127.0.0.1,7778,standard|2,TestWorld,127.0.0.1,7779,pvp"
 * 
 * Error format: ERR|errorCode|errorMessage
 * 
 * Error fields:
 *   - status: "ERR"
 *   - errorCode: machine-readable error code (e.g., "AUTH_FAILED", "SERVER_FULL")
 *   - errorMessage: human-readable error description
 * 
 * Example: "ERR|AUTH_FAILED|Invalid username or password"
 */
std::string buildLoginResponseOkPayload(
    SessionToken token,
    const std::vector<WorldListEntry>& worlds);

std::string buildLoginResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage);

bool parseLoginResponsePayload(
    const std::string& payload,
    LoginResponseData& outData);

// ============================================================================
// WorldAuthRequest / WorldAuthResponse
// ============================================================================

/*
 * WorldAuthRequest (client ? WorldServer)
 * 
 * Payload format: sessionToken|worldId
 * 
 * Fields:
 *   - sessionToken: decimal session token from LoginResponse
 *   - worldId: decimal world ID to authenticate into
 * 
 * Example: "123456789|1"
 */
std::string buildWorldAuthRequestPayload(
    SessionToken sessionToken,
    WorldId worldId);

bool parseWorldAuthRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId);

/*
 * WorldAuthResponse (WorldServer ? client)
 * 
 * Success format: OK|handoffToken|zoneId|zoneHost|zonePort
 * 
 * Success fields:
 *   - status: "OK"
 *   - handoffToken: decimal handoff token for zone server
 *   - zoneId: decimal zone ID to connect to
 *   - zoneHost: zone server hostname/IP
 *   - zonePort: zone server port (decimal)
 * 
 * Example: "OK|987654321|100|127.0.0.1|7779"
 * 
 * Error format: ERR|errorCode|errorMessage
 * 
 * Example: "ERR|INVALID_SESSION|Session token not recognized"
 */
std::string buildWorldAuthResponseOkPayload(
    HandoffToken handoffToken,
    ZoneId zoneId,
    const std::string& zoneHost,
    std::uint16_t zonePort);

std::string buildWorldAuthResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage);

bool parseWorldAuthResponsePayload(
    const std::string& payload,
    WorldAuthResponseData& outData);

// ============================================================================
// ZoneAuthRequest / ZoneAuthResponse
// ============================================================================

/*
 * ZoneAuthRequest (client ? ZoneServer)
 * 
 * Wire Format: handoffToken|characterId
 * Delimiter: pipe character (|)
 * 
 * Fields (in order):
 *   1. handoffToken: decimal string representation of HandoffToken (uint64_t)
 *      - Obtained from WorldAuthResponse or EnterWorldResponse
 *      - Must be non-zero (0 = InvalidHandoffToken)
 *      - Example: "987654321"
 * 
 *   2. characterId: decimal string representation of PlayerId (uint64_t)
 *      - The character to enter the zone with
 *      - Example: "42"
 * 
 * Complete Example: "987654321|42"
 * 
 * Validation Requirements:
 *   - Exactly 2 fields separated by |
 *   - Both fields must parse as unsigned 64-bit integers
 *   - handoffToken must not be 0 (InvalidHandoffToken)
 * 
 * Error Responses:
 *   - PARSE_ERROR: Malformed payload (wrong number of fields or unparseable values)
 *   - INVALID_HANDOFF: handoffToken is 0 or not recognized
 */
std::string buildZoneAuthRequestPayload(
    HandoffToken handoffToken,
    PlayerId characterId);

bool parseZoneAuthRequestPayload(
    const std::string& payload,
    HandoffToken& outHandoffToken,
    PlayerId& outCharacterId);

/*
 * ZoneAuthResponse (ZoneServer ? client)
 * 
 * Success Wire Format: OK|welcomeMessage
 * Error Wire Format: ERR|errorCode|errorMessage
 * Delimiter: pipe character (|)
 * 
 * Success Fields:
 *   1. status: literal string "OK"
 *   2. welcomeMessage: human-readable zone welcome text
 *      - May contain zone name, zone ID, world ID
 *      - Example: "Welcome to East Freeport (zone 10 on world 1)"
 * 
 * Error Fields:
 *   1. status: literal string "ERR"
 *   2. errorCode: machine-readable error code
 *      - PARSE_ERROR: Request payload was malformed
 *      - INVALID_HANDOFF: Handoff token was 0 or not recognized
 *      - HANDOFF_EXPIRED: Token has been used or timed out (future)
 *      - WRONG_ZONE: Token was issued for a different zone (future)
 *   3. errorMessage: human-readable error description
 * 
 * Success Example: "OK|Welcome to Elwynn Forest"
 * Error Example: "ERR|INVALID_HANDOFF|Handoff token not recognized or has expired"
 * 
 * Guarantees:
 *   - ZoneServer ALWAYS sends a ZoneAuthResponse for every ZoneAuthRequest
 *   - Response is either OK or ERR, never silent failure
 *   - All error paths are logged with context
 */
std::string buildZoneAuthResponseOkPayload(
    const std::string& welcomeMessage);

std::string buildZoneAuthResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage);

bool parseZoneAuthResponsePayload(
    const std::string& payload,
    ZoneAuthResponseData& outData);

// ============================================================================
// CharacterListRequest / CharacterListResponse
// ============================================================================

/*
 * CharacterListRequest (client ? WorldServer)
 * 
 * Payload format: sessionToken|worldId
 * 
 * Fields:
 *   - sessionToken: decimal session token from LoginResponse
 *   - worldId: decimal world ID
 * 
 * Example: "123456789|1"
 */
std::string buildCharacterListRequestPayload(
    SessionToken sessionToken,
    WorldId worldId);

bool parseCharacterListRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId);

/*
 * CharacterListResponse (WorldServer ? client)
 * 
 * Success format: OK|characterCount|char1Data|char2Data|...
 * 
 * Character data format (comma-separated): characterId,name,race,class,level
 * 
 * Success fields:
 *   - status: "OK"
 *   - characterCount: number of characters (decimal)
 *   - characterData: comma-separated character entries
 * 
 * Example: "OK|2|1,Arthas,Human,Paladin,5|2,Thrall,Orc,Shaman,3"
 * 
 * Error format: ERR|errorCode|errorMessage
 * 
 * Example: "ERR|INVALID_SESSION|Session token not recognized"
 */
std::string buildCharacterListResponseOkPayload(
    const std::vector<CharacterListEntry>& characters);

std::string buildCharacterListResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage);

bool parseCharacterListResponsePayload(
    const std::string& payload,
    CharacterListResponseData& outData);

// ============================================================================
// CharacterCreateRequest / CharacterCreateResponse
// ============================================================================

/*
 * CharacterCreateRequest (client ? WorldServer)
 * 
 * Payload format: sessionToken|worldId|name|race|class
 * 
 * Fields:
 *   - sessionToken: decimal session token from LoginResponse
 *   - worldId: decimal world ID
 *   - name: character name
 *   - race: character race (e.g., Human, HighElf, DarkElf)
 *   - class: character class (e.g., Warrior, Cleric, ShadowKnight)
 * 
 * Example: "123456789|1|Arthas|Human|Paladin"
 */
std::string buildCharacterCreateRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass);

bool parseCharacterCreateRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId,
    std::string& outName,
    std::string& outRace,
    std::string& outClass);

/*
 * CharacterCreateResponse (WorldServer ? client)
 * 
 * Success format: OK|characterId|name|race|class|level
 * 
 * Success fields:
 *   - status: "OK"
 *   - characterId: newly created character ID (decimal)
 *   - name: character name
 *   - race: character race
 *   - class: character class
 *   - level: starting level (usually 1)
 * 
 * Example: "OK|42|Arthas|Human|Paladin|1"
 * 
 * Error format: ERR|errorCode|errorMessage
 * 
 * Example: "ERR|NAME_TAKEN|Character name already exists"
 */
std::string buildCharacterCreateResponseOkPayload(
    std::uint64_t characterId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass,
    std::uint32_t level);

std::string buildCharacterCreateResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage);

bool parseCharacterCreateResponsePayload(
    const std::string& payload,
    CharacterCreateResponseData& outData);

// ============================================================================
// EnterWorldRequest / EnterWorldResponse
// ============================================================================

/*
 * EnterWorldRequest (client ? WorldServer)
 * 
 * Payload format: sessionToken|worldId|characterId
 * 
 * Fields:
 *   - sessionToken: decimal session token from LoginResponse
 *   - worldId: decimal world ID
 *   - characterId: decimal character ID to enter with
 * 
 * Example: "123456789|1|42"
 */
std::string buildEnterWorldRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    std::uint64_t characterId);

bool parseEnterWorldRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId,
    std::uint64_t& outCharacterId);

/*
 * EnterWorldResponse (WorldServer ? client)
 * 
 * Success format: OK|handoffToken|zoneId|zoneHost|zonePort
 * 
 * Success fields:
 *   - status: "OK"
 *   - handoffToken: decimal handoff token for zone server
 *   - zoneId: decimal zone ID to connect to
 *   - zoneHost: zone server hostname/IP
 *   - zonePort: zone server port (decimal)
 * 
 * Example: "OK|987654321|10|127.0.0.1|7780"
 * 
 * Error format: ERR|errorCode|errorMessage
 * 
 * Example: "ERR|CHARACTER_NOT_FOUND|Character does not exist"
 */
std::string buildEnterWorldResponseOkPayload(
    HandoffToken handoffToken,
    ZoneId zoneId,
    const std::string& zoneHost,
    std::uint16_t zonePort);

std::string buildEnterWorldResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage);

bool parseEnterWorldResponsePayload(
    const std::string& payload,
    EnterWorldResponseData& outData);

// ============================================================================
// Movement Protocol Data Structures
// ============================================================================

/*
 * MovementIntentData
 * 
 * Represents client input for movement.
 * Part of the server-authoritative movement model (GDD Section 14.3).
 * 
 * Important: Client position is NOT trusted. Only input vectors and buttons
 * are sent. The server computes authoritative position and sends back
 * PlayerStateSnapshot messages.
 */
struct MovementIntentData {
    std::uint64_t characterId{ 0 };      // Character sending the input
    std::uint32_t sequenceNumber{ 0 };   // Increments per intent from this client
    float inputX{ 0.0f };                // Movement input X axis: -1.0 to 1.0
    float inputY{ 0.0f };                // Movement input Y axis: -1.0 to 1.0
    float facingYawDegrees{ 0.0f };      // Facing direction: 0-360 degrees
    bool isJumpPressed{ false };         // Jump button state
    std::uint32_t clientTimeMs{ 0 };     // Client timestamp (for debugging/telemetry)
};

/*
 * PlayerStateEntry
 * 
 * Represents a single player's authoritative state from the server.
 */
struct PlayerStateEntry {
    std::uint64_t characterId{ 0 };
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    float velX{ 0.0f };
    float velY{ 0.0f };
    float velZ{ 0.0f };
    float yawDegrees{ 0.0f };
};

/*
 * PlayerStateSnapshotData
 * 
 * Represents the authoritative state of all players in the zone.
 * Part of the server-authoritative movement model (GDD Section 14.3).
 * 
 * The server sends these snapshots periodically (e.g., 20 Hz) to all clients
 * in the zone. Clients use this data to render player positions.
 */
struct PlayerStateSnapshotData {
    std::uint64_t snapshotId{ 0 };                    // Incrementing snapshot identifier
    std::vector<PlayerStateEntry> players;             // All players in this snapshot
};

// ============================================================================
// MovementIntent (client ? ZoneServer)
// ============================================================================

/*
 * MovementIntent (client ? ZoneServer)
 * 
 * Payload format: characterId|sequenceNumber|inputX|inputY|facingYawDegrees|isJumpPressed|clientTimeMs
 * 
 * Fields:
 *   - characterId: decimal character ID sending the input
 *   - sequenceNumber: decimal sequence number (increments per intent)
 *   - inputX: float movement input X axis (-1.0 to 1.0)
 *   - inputY: float movement input Y axis (-1.0 to 1.0)
 *   - facingYawDegrees: float facing direction (0-360 degrees, server normalizes)
 *   - isJumpPressed: 0 or 1 (jump button state)
 *   - clientTimeMs: decimal client timestamp in milliseconds
 * 
 * Example: "42|123|0.5|-1.0|90.0|1|1234567890"
 * 
 * Note: This is part of the server-authoritative movement model.
 *       Client position is NOT sent - only input. Server computes position.
 */
std::string buildMovementIntentPayload(
    const MovementIntentData& data);

bool parseMovementIntentPayload(
    const std::string& payload,
    MovementIntentData& outData);

// ============================================================================
// PlayerStateSnapshot (ZoneServer ? client)
// ============================================================================

/*
 * PlayerStateSnapshot (ZoneServer ? client)
 * 
 * Payload format: snapshotId|playerCount|player1Data|player2Data|...
 * 
 * Player data format (comma-separated): characterId,posX,posY,posZ,velX,velY,velZ,yawDegrees
 * 
 * Fields:
 *   - snapshotId: decimal snapshot identifier (increments per snapshot)
 *   - playerCount: decimal number of players in this snapshot
 *   - playerData: comma-separated player state entries (one per player)
 * 
 * Example: "5|2|42,100.5,200.0,10.0,0.0,0.0,0.0,90.0|43,150.0,200.0,10.0,1.5,0.0,0.0,180.0"
 * 
 * Note: This is the authoritative state from the server.
 *       Clients use this to render player positions, not their own predicted state.
 */
std::string buildPlayerStateSnapshotPayload(
    const PlayerStateSnapshotData& data);

bool parsePlayerStateSnapshotPayload(
    const std::string& payload,
    PlayerStateSnapshotData& outData);

// ============================================================================
// Combat Protocol Data Structures
// ============================================================================

/*
 * AttackRequestData
 * 
 * Represents a client request to attack a target.
 * Part of the server-authoritative combat model.
 * 
 * The server validates the attack (range, cooldown, line-of-sight, etc.)
 * and sends back an AttackResult to all relevant clients.
 */
struct AttackRequestData {
    std::uint64_t attackerCharacterId{ 0 };  // Character performing the attack
    std::uint64_t targetId{ 0 };             // Target ID (NPC or player characterId)
    std::uint32_t abilityId{ 0 };            // 0 = basic attack, >0 = specific ability
    bool isBasicAttack{ true };              // Redundant with abilityId=0, but handy for clarity
};

/*
 * AttackResultData
 * 
 * Represents the server-authoritative result of an attack.
 * Sent to attacker and potentially other nearby clients for combat feedback.
 */
struct AttackResultData {
    std::uint64_t attackerId{ 0 };           // Character who performed the attack
    std::uint64_t targetId{ 0 };             // Target that was attacked
    std::int32_t damage{ 0 };                // Damage dealt (0 if miss/dodge)
    bool wasHit{ false };                    // true = hit, false = miss/dodge/parry
    std::int32_t remainingHp{ 0 };           // Target HP after damage (0 = dead)
    std::int32_t resultCode{ 0 };            // 0 = OK, non-zero = error (out-of-range, cooldown, etc.)
    std::string message;                     // Human-readable summary for logs/client
};

// ============================================================================
// AttackRequest (client ? ZoneServer)
// ============================================================================

/*
 * AttackRequest (client ? ZoneServer)
 * 
 * Payload format: attackerCharacterId|targetId|abilityId|isBasicAttack
 * 
 * Fields:
 *   - attackerCharacterId: decimal character ID performing the attack
 *   - targetId: decimal target ID (NPC or player characterId)
 *   - abilityId: decimal ability ID (0 = basic attack, >0 = specific ability)
 *   - isBasicAttack: 0 or 1 (1 = basic attack, 0 = miss/dodge/parry)
 * 
 * Example: "42|1001|0|1" (character 42 basic-attacks NPC 1001)
 * Example: "42|43|5|0"   (character 42 uses ability 5 on player 43)
 * 
 * Note: Server validates range, cooldown, line-of-sight, etc.
 *       Client should not assume attack will succeed.
 */
std::string buildAttackRequestPayload(
    const AttackRequestData& data);

bool parseAttackRequestPayload(
    const std::string& payload,
    AttackRequestData& outData);

// ============================================================================
// AttackResult (ZoneServer ? client)
// ============================================================================

/*
 * AttackResult (ZoneServer ? client)
 * 
 * Payload format: attackerId|targetId|damage|wasHit|remainingHp|resultCode|message
 * 
 * Fields:
 *   - attackerId: decimal character ID who performed the attack
 *   - targetId: decimal target ID that was attacked
 *   - damage: decimal damage dealt (0 if miss/dodge)
 *   - wasHit: 0 or 1 (1 = hit, 0 = miss/dodge/parry)
 *   - remainingHp: decimal target HP remaining after damage (0 = dead)
 *   - resultCode: decimal result code (0 = OK, non-zero = error)
 *     - 0: Success
 *     - 1: Out of range
 *     - 2: Invalid target
 *     - 3: Ability on cooldown
 *     - 4: Not enough mana/energy
 *     - 5: Target is dead
 *     - 6: Line of sight blocked
 *   - message: human-readable summary (no | characters allowed)
 * 
 * Example: "42|1001|25|1|75|0|Hit for 25 damage" (success, hit for 25, target has 75 HP left)
 * Example: "42|1001|0|0|100|1|Target out of range" (miss, out of range)
 * 
 * Note: Message is sent to attacker and potentially nearby clients for combat log.
 *       resultCode 0 = success, non-zero = various failure conditions.
 */
std::string buildAttackResultPayload(
    const AttackResultData& data);

bool parseAttackResultPayload(
    const std::string& payload,
    AttackResultData& outData);

} // namespace req::shared::protocol
