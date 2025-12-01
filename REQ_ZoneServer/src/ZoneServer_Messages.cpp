#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <string>
#include <type_traits>
#include <chrono>
#include <algorithm>

namespace req::zone {

void ZoneServer::handleMessage(const req::shared::MessageHeader& header,
                               const req::shared::net::Connection::ByteArray& payload,
                               ConnectionPtr connection) {
    // Log incoming message header details
    req::shared::logInfo("zone", std::string{"[RECV] Message header: type="} + 
        std::to_string(static_cast<int>(header.type)) + 
        " (enum: " + std::to_string(static_cast<std::underlying_type_t<req::shared::MessageType>>(header.type)) + ")" +
        ", protocolVersion=" + std::to_string(header.protocolVersion) +
        ", payloadSize=" + std::to_string(header.payloadSize));

    // Check protocol version
    if (header.protocolVersion != req::shared::CurrentProtocolVersion) {
        req::shared::logWarn("zone", std::string{"Protocol version mismatch: client="} + 
            std::to_string(header.protocolVersion) + ", server=" + std::to_string(req::shared::CurrentProtocolVersion));
    }

    std::string body(payload.begin(), payload.end());

    switch (header.type) {
    case req::shared::MessageType::ZoneAuthRequest: {
        /*
         * ZoneAuthRequest Handler
         * 
         * Protocol Schema (from ProtocolSchemas.h):
         *   Payload format: handoffToken|characterId
         *   
         *   Fields:
         *     - handoffToken: decimal handoff token from WorldAuthResponse/EnterWorldResponse
         *     - characterId: decimal character ID to enter zone with
         *   
         *   Example: "987654321|42"
         * 
         * Response:
         *   ZoneAuthResponse with either:
         *     - Success: "OK|<welcomeMessage>"
         *     - Error: "ERR|<errorCode>|<errorMessage>"
         */
        
        req::shared::logInfo("zone", std::string{"[ZONEAUTH] Received ZoneAuthRequest, payloadSize="} + 
            std::to_string(header.payloadSize));
        req::shared::logInfo("zone", std::string{"[ZONEAUTH] Raw payload: '"} + body + "'");

        req::shared::HandoffToken handoffToken = 0;
        req::shared::PlayerId characterId = 0;
        
        // Parse the payload
        if (!req::shared::protocol::parseZoneAuthRequestPayload(body, handoffToken, characterId)) {
            req::shared::logError("zone", "[ZONEAUTH] PARSE FAILED - sending error response");
            
            auto errPayload = req::shared::protocol::buildZoneAuthResponseErrorPayload(
                "PARSE_ERROR", 
                "Malformed zone auth request - expected format: handoffToken|characterId");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            
            req::shared::logInfo("zone", std::string{"[ZONEAUTH] Sending ERROR response: type="} + 
                std::to_string(static_cast<int>(req::shared::MessageType::ZoneAuthResponse)) +
                ", payload='" + errPayload + "'");

            connection->send(req::shared::MessageType::ZoneAuthResponse, errBytes);
            return;
        }

        req::shared::logInfo("zone", std::string{"[ZONEAUTH] Parsed successfully:"});
        req::shared::logInfo("zone", std::string{"[ZONEAUTH]   handoffToken="} + std::to_string(handoffToken));
        req::shared::logInfo("zone", std::string{"[ZONEAUTH]   characterId="} + std::to_string(characterId));
        req::shared::logInfo("zone", std::string{"[ZONEAUTH]   zone=\""} + zoneName_ + "\" (id=" + std::to_string(zoneId_) + ")");

        // TODO: Validate handoffToken with world server or shared handoff service
        // For now, we use stub validation that accepts any non-zero token
        req::shared::logInfo("zone", "[ZONEAUTH] Validating handoff token (using stub validation - TODO: integrate with session service)");
        
        if (handoffToken == req::shared::InvalidHandoffToken) {
            req::shared::logWarn("zone", std::string{"[ZONEAUTH] INVALID handoff token (value="} + 
                std::to_string(handoffToken) + ") - sending error response");
            
            auto errPayload = req::shared::protocol::buildZoneAuthResponseErrorPayload(
                "INVALID_HANDOFF", 
                "Handoff token not recognized or has expired");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            
            req::shared::logInfo("zone", std::string{"[ZONEAUTH] Sending ERROR response: payload='"} + errPayload + "'");
            connection->send(req::shared::MessageType::ZoneAuthResponse, errBytes);
            return;
        }

        // TODO: Additional validation checks:
        // - Verify handoff token hasn't been used already
        // - Verify handoff token was issued for this specific zone
        // - Load character data from database
        // - Verify character belongs to the session associated with the handoff token
        // - Check zone capacity/population limits
        
        req::shared::logInfo("zone", "[ZONEAUTH] Handoff token validation PASSED (stub)");
        
        // Load character data from disk
        req::shared::logInfo("zone", std::string{"[ZONEAUTH] Loading character data: characterId="} +
            std::to_string(characterId));
        
        auto character = characterStore_.loadById(characterId);
        if (!character.has_value()) {
            req::shared::logError("zone", std::string{"[ZONEAUTH] CHARACTER NOT FOUND: characterId="} +
                std::to_string(characterId) + " - sending error response");
            
            auto errPayload = req::shared::protocol::buildZoneAuthResponseErrorPayload(
                "CHARACTER_NOT_FOUND",
                "Character data could not be loaded");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::ZoneAuthResponse, errBytes);
            return;
        }
        
        req::shared::logInfo("zone", std::string{"[ZONEAUTH] Character loaded: name="} + character->name +
            ", race=" + character->race + ", class=" + character->characterClass +
            ", level=" + std::to_string(character->level));
        
        // Create ZonePlayer entry
        req::shared::logInfo("zone", std::string{"[ZONEAUTH] Creating ZonePlayer entry for characterId="} + 
            std::to_string(characterId));
        
        // Check if character is already in zone (duplicate login or reconnect)
        auto existingIt = players_.find(characterId);
        if (existingIt != players_.end()) {
            req::shared::logWarn("zone", std::string{"[ZONEAUTH] Character already in zone: characterId="} +
                std::to_string(characterId) + ", removing old entry");
            removePlayer(characterId);
        }
        
        ZonePlayer player;
        player.characterId = characterId;
        player.accountId = character->accountId;
        player.connection = connection;
        
        // Determine spawn position using character data
        spawnPlayer(*character, player);
        
        // Initialize combat state from character
        player.level = character->level;
        player.hp = (character->hp > 0) ? character->hp : character->maxHp;
        player.maxHp = character->maxHp;
        player.mana = (character->mana > 0) ? character->mana : character->maxMana;
        player.maxMana = character->maxMana;
        
        // Initialize primary stats
        player.strength = character->strength;
        player.stamina = character->stamina;
        player.agility = character->agility;
        player.dexterity = character->dexterity;
        player.intelligence = character->intelligence;
        player.wisdom = character->wisdom;
        player.charisma = character->charisma;
        
        req::shared::logInfo("zone", std::string{"[COMBAT] Initialized combat state: level="} +
            std::to_string(player.level) + ", hp=" + std::to_string(player.hp) + "/" +
            std::to_string(player.maxHp) + ", mana=" + std::to_string(player.mana) + "/" +
            std::to_string(player.maxMana));
        
        // Initialize last valid position
        player.lastValidPosX = player.posX;
        player.lastValidPosY = player.posY;
        player.lastValidPosZ = player.posZ;
        
        // Initialize input state
        player.inputX = 0.0f;
        player.inputY = 0.0f;
        player.isJumpPressed = false;
        player.lastSequenceNumber = 0;
        player.isInitialized = true;
        player.isDirty = false;
        player.combatStatsDirty = false;
        
        // Insert into players map
        players_[characterId] = player;
        connectionToCharacterId_[connection] = characterId;
        
        req::shared::logInfo("zone", std::string{"[ZonePlayer created] characterId="} + 
            std::to_string(characterId) + ", accountId=" + std::to_string(character->accountId) +
            ", zoneId=" + std::to_string(zoneId_) + ", pos=(" + 
            std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + 
            std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees) +
            ", active_players=" + std::to_string(players_.size()));

        // Build success response
        std::string welcomeMsg = std::string("Welcome to ") + zoneName_ + 
            " (zone " + std::to_string(zoneId_) + " on world " + std::to_string(worldId_) + ")";
        
        auto respPayload = req::shared::protocol::buildZoneAuthResponseOkPayload(welcomeMsg);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        
        req::shared::logInfo("zone", std::string{"[ZONEAUTH] Sending SUCCESS response:"});
        req::shared::logInfo("zone", std::string{"[ZONEAUTH]   type="} + 
            std::to_string(static_cast<int>(req::shared::MessageType::ZoneAuthResponse)) + 
            " (enum: " + std::to_string(static_cast<std::underlying_type_t<req::shared::MessageType>>(req::shared::MessageType::ZoneAuthResponse)) + ")");

        req::shared::logInfo("zone", std::string{"[ZONEAUTH]   payloadSize="} + std::to_string(respPayload.size()));
        req::shared::logInfo("zone", std::string{"[ZONEAUTH]   payload='"} + respPayload + "'");

        connection->send(req::shared::MessageType::ZoneAuthResponse, respBytes);

        req::shared::logInfo("zone", std::string{"[ZONEAUTH] COMPLETE: characterId="} + 
            std::to_string(characterId) + " successfully entered zone \"" + zoneName_ + "\"");
        break;
    }
    
    case req::shared::MessageType::MovementIntent: {
        // Log raw payload for debugging
        req::shared::logInfo("zone", std::string{"[Movement] Raw payload: '"} + body + "'");
        
        req::shared::protocol::MovementIntentData intent;
        
        if (!req::shared::protocol::parseMovementIntentPayload(body, intent)) {
            // Parse failed - log with rate limiting to prevent spam
            static std::uint64_t parseErrorCount = 0;
            static std::chrono::steady_clock::time_point lastLogTime = std::chrono::steady_clock::now();
            
            parseErrorCount++;
            
            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastLog = std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime).count();
            
            if (timeSinceLastLog >= 5) {  // Log summary every 5 seconds
                req::shared::logError("zone", std::string{"Failed to parse MovementIntent payload (errors in last 5s: "} + 
                    std::to_string(parseErrorCount) + "), last payload: '" + body + "'");
                parseErrorCount = 0;
                lastLogTime = now;
            }
            
            // IMPORTANT: Do NOT use 'intent' beyond this point - it contains garbage
            // Safe return without touching player state
            return;
        }
        
        // Log parsed MovementIntent details
        req::shared::logInfo("zone", std::string{"[Movement] Parsed Intent: charId="} + 
            std::to_string(intent.characterId) + ", seq=" + std::to_string(intent.sequenceNumber) +
            ", input=(" + std::to_string(intent.inputX) + "," + std::to_string(intent.inputY) + ")" +
            ", yaw=" + std::to_string(intent.facingYawDegrees) +
            ", jump=" + (intent.isJumpPressed ? "1" : "0") +
            ", clientTimeMs=" + std::to_string(intent.clientTimeMs));
        
        // Find the corresponding ZonePlayer
        auto it = players_.find(intent.characterId);
        if (it == players_.end()) {
            req::shared::logWarn("zone", std::string{"MovementIntent for unknown characterId="} + 
                std::to_string(intent.characterId) + " (player not in zone or already disconnected)");
            return;
        }
        
        ZonePlayer& player = it->second;
        
        // Verify connection is still valid
        if (!player.connection) {
            req::shared::logWarn("zone", std::string{"MovementIntent for characterId="} +
                std::to_string(intent.characterId) + " but connection is null (disconnecting?)");
            return;
        }
        
        // Verify this message came from the correct connection
        if (player.connection != connection) {
            req::shared::logWarn("zone", std::string{"MovementIntent for characterId="} +
                std::to_string(intent.characterId) + " from wrong connection (possible hijack attempt)");
            return;
        }
        
        // Verify player is initialized
        if (!player.isInitialized) {
            req::shared::logWarn("zone", std::string{"MovementIntent for uninitialized characterId="} +
                std::to_string(intent.characterId));
            return;
        }
        
        // Validate sequence number (ignore old/duplicate packets)
        if (intent.sequenceNumber <= player.lastSequenceNumber) {
            // This is normal for out-of-order packets, don't log at warn level
            return;
        }
        
        // Update player input state (clamped and normalized)
        player.inputX = std::clamp(intent.inputX, -1.0f, 1.0f);
        player.inputY = std::clamp(intent.inputY, -1.0f, 1.0f);
        player.isJumpPressed = intent.isJumpPressed;
        
        // Normalize yaw to 0-360
        player.yawDegrees = intent.facingYawDegrees;
        while (player.yawDegrees < 0.0f) player.yawDegrees += 360.0f;
        while (player.yawDegrees >= 360.0f) player.yawDegrees -= 360.0f;
        
        player.lastSequenceNumber = intent.sequenceNumber;
        
        // Log that input was stored
        req::shared::logInfo("zone", std::string{"[Movement] Stored input for charId="} +
            std::to_string(intent.characterId) + ": input=(" + 
            std::to_string(player.inputX) + "," + std::to_string(player.inputY) + ")" +
            ", yaw=" + std::to_string(player.yawDegrees) +
            ", currentPos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")");
        
        break;
    }
    
    case req::shared::MessageType::PlayerStateSnapshot: {
        // Server -> Client only; ignore if client sends it
        req::shared::logWarn("zone", "Received PlayerStateSnapshot from client (invalid direction)");
        break;
    }
    
    case req::shared::MessageType::AttackRequest: {
        req::shared::protocol::AttackRequestData request;
        
        if (!req::shared::protocol::parseAttackRequestPayload(body, request)) {
            req::shared::logError("zone", "Failed to parse AttackRequest payload");
            return;
        }
        
        req::shared::logInfo("zone", std::string{"[COMBAT] AttackRequest: attackerCharId="} +
            std::to_string(request.attackerCharacterId) + ", targetId=" +
            std::to_string(request.targetId) + ", abilityId=" + std::to_string(request.abilityId) +
            ", basicAttack=" + (request.isBasicAttack ? "1" : "0"));
        
        // Validate attacker is a valid player
        auto attackerIt = players_.find(request.attackerCharacterId);
        if (attackerIt == players_.end()) {
            req::shared::logWarn("zone", std::string{"[COMBAT] Invalid attacker: characterId="} +
                std::to_string(request.attackerCharacterId) + " not found (disconnected or never entered zone)");
            
            // Send error response if connection is still valid
            if (connection) {
                req::shared::protocol::AttackResultData result;
                result.attackerId = request.attackerCharacterId;
                result.targetId = request.targetId;
                result.damage = 0;
                result.wasHit = false;
                result.remainingHp = 0;
                result.resultCode = 2; // Not owner / invalid attacker
                result.message = "Invalid attacker";
                
                try {
                    std::string resultPayload = req::shared::protocol::buildAttackResultPayload(result);
                    req::shared::net::Connection::ByteArray resultBytes(resultPayload.begin(), resultPayload.end());
                    connection->send(req::shared::MessageType::AttackResult, resultBytes);
                } catch (const std::exception& e) {
                    req::shared::logError("zone", std::string{"[COMBAT] Failed to send error response: "} + e.what());
                }
            }
            return;
        }
        
        // Validate connection owns the attacker
        auto& attacker = attackerIt->second;
        
        // Check if connection is still valid
        if (!attacker.connection) {
            req::shared::logWarn("zone", std::string{"[COMBAT] Attacker connection is null: characterId="} +
                std::to_string(request.attackerCharacterId));
            return;
        }
        
        if (attacker.connection != connection) {
            req::shared::logWarn("zone", std::string{"[COMBAT] Connection doesn't own attacker: characterId="} +
                std::to_string(request.attackerCharacterId) + " (possible hijack attempt)");
            
            // Send error response
            try {
                req::shared::protocol::AttackResultData result;
                result.attackerId = request.attackerCharacterId;
                result.targetId = request.targetId;
                result.damage = 0;
                result.wasHit = false;
                result.remainingHp = 0;
                result.resultCode = 2; // Not owner
                result.message = "Not your character";
                
                std::string resultPayload = req::shared::protocol::buildAttackResultPayload(result);
                req::shared::net::Connection::ByteArray resultBytes(resultPayload.begin(), resultPayload.end());
                connection->send(req::shared::MessageType::AttackResult, resultBytes);
            } catch (const std::exception& e) {
                req::shared::logError("zone", std::string{"[COMBAT] Failed to send error response: "} + e.what());
            }
            return;
        }
        
        // Find target NPC
        auto targetIt = npcs_.find(request.targetId);
        if (targetIt == npcs_.end()) {
            req::shared::logWarn("zone", std::string{"[COMBAT] Invalid target: npcId="} +
                std::to_string(request.targetId) + " not found");
            
            try {
                req::shared::protocol::AttackResultData result;
                result.attackerId = request.attackerCharacterId;
                result.targetId = request.targetId;
                result.damage = 0;
                result.wasHit = false;
                result.remainingHp = 0;
                result.resultCode = 1; // Invalid target
                result.message = "Invalid target";
                
                std::string resultPayload = req::shared::protocol::buildAttackResultPayload(result);
                req::shared::net::Connection::ByteArray resultBytes(resultPayload.begin(), resultPayload.end());
                connection->send(req::shared::MessageType::AttackResult, resultBytes);
            } catch (const std::exception& e) {
                req::shared::logError("zone", std::string{"[COMBAT] Failed to send error response: "} + e.what());
            }
            return;
        }
        
        // Process the attack (wrapped in try-catch in processAttack)
        try {
            processAttack(attacker, targetIt->second, request);
        } catch (const std::exception& e) {
            req::shared::logError("zone", std::string{"[COMBAT] Exception during processAttack: "} + e.what());
        }
        break;
    }
    
    case req::shared::MessageType::DevCommand: {
        req::shared::protocol::DevCommandData devCmd;
        
        if (!req::shared::protocol::parseDevCommandPayload(body, devCmd)) {
            req::shared::logError("zone", "[DEV] Failed to parse DevCommand payload");
            
            // Send error response
            req::shared::protocol::DevCommandResponseData response;
            response.success = false;
            response.message = "Failed to parse dev command";
            
            std::string respPayload = req::shared::protocol::buildDevCommandResponsePayload(response);
            req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
            connection->send(req::shared::MessageType::DevCommandResponse, respBytes);
            return;
        }
        
        req::shared::logInfo("zone", std::string{"[DEV] DevCommand: charId="} +
            std::to_string(devCmd.characterId) + ", command=" + devCmd.command +
            ", param1=" + devCmd.param1 + ", param2=" + devCmd.param2);
        
        req::shared::protocol::DevCommandResponseData response;
        response.success = true;
        
        // Process command
        if (devCmd.command == "suicide") {
            devSuicide(devCmd.characterId);
            response.message = "Character forced to 0 HP and death triggered";
        } else if (devCmd.command == "givexp") {
            try {
                std::int64_t amount = std::stoll(devCmd.param1);
                devGiveXp(devCmd.characterId, amount);
                response.message = "Gave " + std::to_string(amount) + " XP";
            } catch (...) {
                response.success = false;
                response.message = "Invalid XP amount: " + devCmd.param1;
            }
        } else if (devCmd.command == "setlevel") {
            try {
                std::uint32_t level = std::stoul(devCmd.param1);
                devSetLevel(devCmd.characterId, level);
                response.message = "Set level to " + std::to_string(level);
            } catch (...) {
                response.success = false;
                response.message = "Invalid level: " + devCmd.param1;
            }
        } else if (devCmd.command == "respawn") {
            auto playerIt = players_.find(devCmd.characterId);
            if (playerIt != players_.end()) {
                respawnPlayer(playerIt->second);
                response.message = "Player respawned at bind point";
            } else {
                response.success = false;
                response.message = "Player not found in zone";
            }
        } else {
            response.success = false;
            response.message = "Unknown command: " + devCmd.command;
        }
        
        // Send response
        std::string respPayload = req::shared::protocol::buildDevCommandResponsePayload(response);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        connection->send(req::shared::MessageType::DevCommandResponse, respBytes);
        
        break;
    }
    
    default:
        req::shared::logWarn("zone", std::string{"Unsupported message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        break;
    }
}

} // namespace req::zone
