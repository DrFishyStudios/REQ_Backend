#pragma once

#include <string>
#include <cstdint>

#include "Types.h"

/*
 * Protocol_World.h
 * 
 * World protocol definitions for REQ backend handshake.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
 */

namespace req::shared::protocol {

// ============================================================================
// Data Structures for Parsed Payloads
// ============================================================================

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

} // namespace req::shared::protocol
