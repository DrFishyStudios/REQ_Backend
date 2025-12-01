#pragma once

#include <string>
#include <cstdint>

/*
 * Protocol_Dev.h
 * 
 * Dev command protocol definitions for REQ backend.
 * These are dev-only commands for testing death, XP, levels, etc.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
 */

namespace req::shared::protocol {

// ============================================================================
// Data Structures for Parsed Payloads
// ============================================================================

/*
 * DevCommandData
 * 
 * Represents a dev command sent from client to server.
 */
struct DevCommandData {
    std::uint64_t characterId{ 0 };  // Character to apply command to
    std::string command;              // Command name (suicide, givexp, setlevel, respawn)
    std::string param1;               // First parameter (if any)
    std::string param2;               // Second parameter (if any)
};

/*
 * DevCommandResponseData
 * 
 * Represents the server's response to a dev command.
 */
struct DevCommandResponseData {
    bool success{ false };            // true if command succeeded
    std::string message;              // Human-readable response message
};

// ============================================================================
// DevCommand (client ? ZoneServer)
// ============================================================================

/*
 * DevCommand (client ? ZoneServer)
 * 
 * Payload format: characterId|command|param1|param2
 * 
 * Fields:
 *   - characterId: decimal character ID to apply command to
 *   - command: command name (suicide, givexp, setlevel, respawn)
 *   - param1: first parameter (optional, empty string if not used)
 *   - param2: second parameter (optional, empty string if not used)
 * 
 * Example: "42|suicide||" (character 42 commits suicide)
 * Example: "42|givexp|1000|" (give character 42 1000 XP)
 * Example: "42|setlevel|10|" (set character 42 to level 10)
 * Example: "42|respawn||" (respawn character 42 at bind point)
 */
std::string buildDevCommandPayload(
    const DevCommandData& data);

bool parseDevCommandPayload(
    const std::string& payload,
    DevCommandData& outData);

// ============================================================================
// DevCommandResponse (ZoneServer ? client)
// ============================================================================

/*
 * DevCommandResponse (ZoneServer ? client)
 * 
 * Payload format: success|message
 * 
 * Fields:
 *   - success: 0 or 1 (1 = success, 0 = failure)
 *   - message: human-readable response (no | characters allowed)
 * 
 * Example: "1|Character died and lost XP" (success)
 * Example: "0|Player not found" (failure)
 */
std::string buildDevCommandResponsePayload(
    const DevCommandResponseData& data);

bool parseDevCommandResponsePayload(
    const std::string& payload,
    DevCommandResponseData& outData);

} // namespace req::shared::protocol
