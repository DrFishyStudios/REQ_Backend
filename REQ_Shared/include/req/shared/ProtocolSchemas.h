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
 * Payload format: handoffToken|characterId
 * 
 * Fields:
 *   - handoffToken: decimal handoff token from WorldAuthResponse
 *   - characterId: decimal character ID to enter zone with
 * 
 * Example: "987654321|42"
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
 * Success format: OK|welcomeMessage
 * 
 * Success fields:
 *   - status: "OK"
 *   - welcomeMessage: zone welcome text
 * 
 * Example: "OK|Welcome to Elwynn Forest"
 * 
 * Error format: ERR|errorCode|errorMessage
 * 
 * Example: "ERR|INVALID_HANDOFF|Handoff token not recognized"
 */
std::string buildZoneAuthResponseOkPayload(
    const std::string& welcomeMessage);

std::string buildZoneAuthResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage);

bool parseZoneAuthResponsePayload(
    const std::string& payload,
    ZoneAuthResponseData& outData);

} // namespace req::shared::protocol
