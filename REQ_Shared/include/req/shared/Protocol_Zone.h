#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Types.h"

/*
 * Protocol_Zone.h
 * 
 * Zone protocol definitions for REQ backend.
 * Includes zone authentication and movement/state synchronization.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
 */

namespace req::shared::protocol {

// ============================================================================
// Data Structures for Parsed Payloads
// ============================================================================

struct ZoneAuthResponseData {
    bool success{ false };
    
    // Success fields
    std::string welcomeMessage;
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};

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
    std::uint64_t clientTimeMs{ 0 };     // Client timestamp (for debugging/telemetry) - 64-bit to handle large values
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

/*
 * EntitySpawnData
 * 
 * Represents an entity (player or NPC) spawning in the zone.
 * Sent when a player enters the zone (for all existing entities) or when a new NPC spawns.
 * 
 * Entity Types:
 *   0 = Player (characterId is the player's character ID)
 *   1 = NPC (characterId is the NPC instance ID)
 */
struct EntitySpawnData {
    std::uint64_t entityId{ 0 };          // Unique entity identifier (characterId for players, npcId for NPCs)
    std::uint32_t entityType{ 0 };        // 0=Player, 1=NPC
    std::uint32_t templateId{ 0 };        // Template/model ID (for NPCs, the NPC template ID; for players, race ID)
    std::string name;                      // Display name
    float posX{ 0.0f };                   // Spawn position X
    float posY{ 0.0f };                   // Spawn position Y
    float posZ{ 0.0f };                   // Spawn position Z
    float heading{ 0.0f };                // Facing direction (0-360 degrees)
    std::uint32_t level{ 1 };             // Entity level
    std::int32_t hp{ 100 };               // Current HP
    std::int32_t maxHp{ 100 };            // Maximum HP
    std::string visualId;                  // Visual/model ID for client rendering (from NPC template)
};

/*
 * EntityUpdateData
 * 
 * Represents a positional and state update for an entity.
 * Sent periodically for NPCs at server tick rate (e.g., 5-10 Hz).
 * Includes position, heading, and HP for interpolation/display.
 */
struct EntityUpdateData {
    std::uint64_t entityId{ 0 };          // Entity identifier
    float posX{ 0.0f };                   // Current position X
    float posY{ 0.0f };                   // Current position Y
    float posZ{ 0.0f };                   // Current position Z
    float heading{ 0.0f };                // Facing direction (0-360 degrees)
    std::int32_t hp{ 100 };               // Current HP
    std::uint8_t state{ 0 };              // Entity state (0=Idle, 1=Combat, 2=Dead, etc.)
};

/*
 * EntityDespawnData
 * 
 * Represents an entity leaving the zone or dying.
 * Sent when an NPC dies, respawns, or when a player disconnects.
 * 
 * Despawn Reasons:
 *   0 = Disconnect (player logged out)
 *   1 = Death (entity died)
 *   2 = Despawn (NPC respawn cycle)
 *   3 = OutOfRange (entity left interest radius)
 */
struct EntityDespawnData {
    std::uint64_t entityId{ 0 };          // Entity identifier
    std::uint32_t reason{ 0 };            // Despawn reason code
};

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
// EntitySpawn (ZoneServer ? client)
// ============================================================================

/*
 * EntitySpawn (ZoneServer ? client)
 * 
 * Payload format: entityId|entityType|templateId|name|posX|posY|posZ|heading|level|hp|maxHp|visualId
 * 
 * Fields:
 *   - entityId: decimal entity identifier (characterId for players, npcId for NPCs)
 *   - entityType: 0=Player, 1=NPC
 *   - templateId: template/model ID (NPC template ID for NPCs, race ID for players)
 *   - name: display name (may contain spaces)
 *   - posX, posY, posZ: spawn position floats
 *   - heading: facing direction (0-360 degrees)
 *   - level: entity level (uint32)
 *   - hp: current HP (int32)
 *   - maxHp: maximum HP (int32)
 *   - visualId: visual/model ID string for client rendering (from NPC template or character)
 * 
 * Example: "1001|1|5001|A Decaying Skeleton|100.0|50.0|0.0|90.0|1|20|20|skeleton_01"
 * 
 * Note: Sent to player when entering zone (for all existing entities) and when new entities spawn.
 */
std::string buildEntitySpawnPayload(
    const EntitySpawnData& data);

bool parseEntitySpawnPayload(
    const std::string& payload,
    EntitySpawnData& outData);

// ============================================================================
// EntityUpdate (ZoneServer ? client)
// ============================================================================

/*
 * EntityUpdate (ZoneServer ? client)
 * 
 * Payload format: entityId|posX|posY|posZ|heading|hp|state
 * 
 * Fields:
 *   - entityId: decimal entity identifier
 *   - posX, posY, posZ: current position floats
 *   - heading: facing direction (0-360 degrees)
 *   - hp: current HP (int32)
 *   - state: entity state (0=Idle, 1=Combat, 2=Dead, etc.)
 * 
 * Example: "1001|105.5|52.3|0.0|95.0|15|1"
 * 
 * Note: Sent periodically for NPCs (e.g., 5-10 Hz) for position and HP updates.
 */
std::string buildEntityUpdatePayload(
    const EntityUpdateData& data);

bool parseEntityUpdatePayload(
    const std::string& payload,
    EntityUpdateData& outData);

// ============================================================================
// EntityDespawn (ZoneServer ? client)
// ============================================================================

/*
 * EntityDespawn (ZoneServer ? client)
 * 
 * Payload format: entityId|reason
 * 
 * Fields:
 *   - entityId: decimal entity identifier
 *   - reason: despawn reason (0=Disconnect, 1=Death, 2=Despawn, 3=OutOfRange)
 * 
 * Example: "1001|1" (NPC 1001 died)
 * 
 * Note: Sent when entity leaves zone, dies, or exits client's interest radius.
 */
std::string buildEntityDespawnPayload(
    const EntityDespawnData& data);

bool parseEntityDespawnPayload(
    const std::string& payload,
    EntityDespawnData& outData);

} // namespace req::shared::protocol
