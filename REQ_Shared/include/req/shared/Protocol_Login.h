#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Types.h"

/*
 * Protocol_Login.h
 * 
 * Login protocol definitions for REQ backend handshake.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
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

} // namespace req::shared::protocol
