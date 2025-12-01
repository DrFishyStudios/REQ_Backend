#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Types.h"

/*
 * Protocol_Character.h
 * 
 * Character protocol definitions for REQ backend.
 * Includes character list, creation, and world entry.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
 */

namespace req::shared::protocol {

// ============================================================================
// Data Structures for Parsed Payloads
// ============================================================================

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

} // namespace req::shared::protocol
