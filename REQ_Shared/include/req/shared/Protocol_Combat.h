#pragma once

#include <string>
#include <cstdint>

/*
 * Protocol_Combat.h
 * 
 * Combat protocol definitions for REQ backend.
 * Includes attack requests and results.
 * All payloads are UTF-8 strings with pipe (|) delimiters.
 */

namespace req::shared::protocol {

// ============================================================================
// Data Structures for Parsed Payloads
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
