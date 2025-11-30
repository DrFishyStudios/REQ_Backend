#include "../include/req/zone/ZoneServer.h"

#include <cmath>
#include <algorithm>
#include <fstream>
#include <random>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

namespace req::zone {

namespace {
    // Simulation constants
    constexpr float TICK_RATE_HZ = 20.0f;                    // 20 ticks per second
    constexpr auto TICK_INTERVAL_MS = std::chrono::milliseconds(50);  // 50ms per tick
    constexpr float TICK_DT = 1.0f / TICK_RATE_HZ;           // 0.05 seconds
    
    // TODO: configurable movement speed (now using zoneConfig_.moveSpeed instead)
    // Old default was 7.0f - now defaults to 70.0f for visible movement
    
    constexpr float GRAVITY = -30.0f;                        // units per second squared
    constexpr float JUMP_VELOCITY = 10.0f;                   // initial jump velocity
    constexpr float GROUND_LEVEL = 0.0f;                     // Z coordinate of ground
    
    constexpr float MAX_ALLOWED_MOVE_MULTIPLIER = 1.5f;     // slack for network jitter
    constexpr float SUSPICIOUS_MOVE_MULTIPLIER = 5.0f;      // clearly insane movement
}

ZoneServer::ZoneServer(std::uint32_t worldId,
                       std::uint32_t zoneId,
                       const std::string& zoneName,
                       const std::string& address,
                       std::uint16_t port,
                       const std::string& charactersPath)
    : acceptor_(ioContext_), tickTimer_(ioContext_), autosaveTimer_(ioContext_),
      worldId_(worldId), zoneId_(zoneId), zoneName_(zoneName), 
      address_(address), port_(port), characterStore_(charactersPath) {
    using boost::asio::ip::tcp;
    boost::system::error_code ec;
    tcp::endpoint endpoint(boost::asio::ip::make_address(address_, ec), port_);
    if (ec) {
        req::shared::logError("zone", std::string{"Invalid listen address: "} + ec.message());
        return;
    }
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) { req::shared::logError("zone", std::string{"acceptor open failed: "} + ec.message()); }
    acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    if (ec) { req::shared::logError("zone", std::string{"acceptor bind failed: "} + ec.message()); }
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) { req::shared::logError("zone", std::string{"acceptor listen failed: "} + ec.message()); }
    
    // Initialize zone config with defaults
    zoneConfig_.zoneId = zoneId;
    zoneConfig_.zoneName = zoneName;
    zoneConfig_.safeX = 0.0f;
    zoneConfig_.safeY = 0.0f;
    zoneConfig_.safeZ = 0.0f;
    zoneConfig_.safeYaw = 0.0f;
    zoneConfig_.moveSpeed = 70.0f;
    zoneConfig_.autosaveIntervalSec = 30.0f;
    zoneConfig_.broadcastFullState = true;
    zoneConfig_.interestRadius = 2000.0f;
    zoneConfig_.debugInterest = false;
    
    // Attempt to load zone config from JSON (optional)
    std::string configPath = "config/zones/zone_" + std::to_string(zoneId) + "_config.json";
    try {
        auto loadedConfig = req::shared::loadZoneConfig(configPath);
        
        // Verify zone ID matches
        if (loadedConfig.zoneId != zoneId) {
            req::shared::logWarn("zone", std::string{"Zone config file zone_id ("} + 
                std::to_string(loadedConfig.zoneId) + ") does not match server zone_id (" + 
                std::to_string(zoneId) + "), using defaults");
        } else {
            zoneConfig_ = loadedConfig;
            req::shared::logInfo("zone", std::string{"Loaded zone config from: "} + configPath);
        }
    } catch (const std::exception& e) {
        req::shared::logInfo("zone", std::string{"Zone config not found or invalid ("} + configPath + 
            "), using defaults");
    }
    
    // Log ZoneServer construction
    req::shared::logInfo("zone", std::string{"ZoneServer constructed:"});
    req::shared::logInfo("zone", std::string{"  worldId="} + std::to_string(worldId_));
    req::shared::logInfo("zone", std::string{"  zoneId="} + std::to_string(zoneId_));
    req::shared::logInfo("zone", std::string{"  zoneName=\""} + zoneName_ + "\"");
    req::shared::logInfo("zone", std::string{"  address="} + address_);
    req::shared::logInfo("zone", std::string{"  port="} + std::to_string(port_));
    req::shared::logInfo("zone", std::string{"  charactersPath="} + charactersPath);
    req::shared::logInfo("zone", std::string{"  tickRate="} + std::to_string(TICK_RATE_HZ) + " Hz");
    req::shared::logInfo("zone", std::string{"  moveSpeed="} + std::to_string(zoneConfig_.moveSpeed) + " uu/s");
    req::shared::logInfo("zone", std::string{"  broadcastFullState="} + 
        (zoneConfig_.broadcastFullState ? "true" : "false"));
    req::shared::logInfo("zone", std::string{"  interestRadius="} + std::to_string(zoneConfig_.interestRadius));
}

void ZoneServer::run() {
    req::shared::logInfo("zone", std::string{"ZoneServer starting: worldId="} + 
        std::to_string(worldId_) + ", zoneId=" + std::to_string(zoneId_) + 
        ", zoneName=\"" + zoneName_ + "\", address=" + address_ + 
        ", port=" + std::to_string(port_));
    
    // Load NPCs for this zone
    loadNpcsForZone();
    
    startAccept();
    
    // Start the simulation tick loop
    scheduleNextTick();
    req::shared::logInfo("zone", "Simulation tick loop started");
    
    // Start position auto-save timer
    scheduleAutosave();
    req::shared::logInfo("zone", std::string{"Position autosave enabled: interval="} +
        std::to_string(zoneConfig_.autosaveIntervalSec) + "s");
    
    req::shared::logInfo("zone", "Entering IO event loop...");
    ioContext_.run();
}

void ZoneServer::stop() {
    req::shared::logInfo("zone", "ZoneServer shutdown requested");
    ioContext_.stop();
}

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
    
    default:
        req::shared::logWarn("zone", std::string{"Unsupported message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        break;
    }
}

void ZoneServer::scheduleNextTick() {
    tickTimer_.expires_after(TICK_INTERVAL_MS);
    tickTimer_.async_wait([this](const boost::system::error_code& ec) {
        onTick(ec);
    });
}

void ZoneServer::onTick(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
        req::shared::logInfo("zone", "Tick timer cancelled (server shutting down)");
        return;
    }
    
    if (ec) {
        req::shared::logError("zone", std::string{"Tick timer error: "} + ec.message());
        return;
    }
    
    // Update simulation with fixed timestep
    updateSimulation(TICK_DT);
    
    // Broadcast state snapshots to all clients
    broadcastSnapshots();
    
    // Schedule next tick
    scheduleNextTick();
}

void ZoneServer::updateSimulation(float dt) {
    // Log simulation update periodically (every 20 ticks = 1 second at 20Hz)
    static std::uint64_t simLogCounter = 0;
    bool doDetailedLog = (++simLogCounter % 20 == 0);
    
    // Get configurable move speed from zone config
    const float moveSpeed = zoneConfig_.moveSpeed;
    
    // Update player physics
    for (auto& [characterId, player] : players_) {
        if (!player.isInitialized) {
            continue;
        }
        
        // Log initial state for debugging
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) +
                " BEFORE: pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")" +
                ", input=(" + std::to_string(player.inputX) + "," + std::to_string(player.inputY) + ")");
        }
        
        // Compute 2D movement direction from input
        float inputLength = std::sqrt(player.inputX * player.inputX + player.inputY * player.inputY);
        float dirX = player.inputX;
        float dirY = player.inputY;
        
        // Normalize diagonal movement
        if (inputLength > 1.0f) {
            dirX /= inputLength;
            dirY /= inputLength;
            inputLength = 1.0f;
        }
        
        // Compute desired velocity using configurable move speed
        player.velX = dirX * moveSpeed;
        player.velY = dirY * moveSpeed;
        
        // Compute expected move distance this frame
        const float maxMoveDist = moveSpeed * dt;
        
        // Log velocity computation with move details
        if (doDetailedLog && (std::abs(player.velX) > 0.01f || std::abs(player.velY) > 0.01f)) {
            req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) +
                " MOVE: pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")" +
                ", input=(" + std::to_string(dirX) + "," + std::to_string(dirY) + ")" +
                ", moveSpeed=" + std::to_string(moveSpeed) +
                ", dt=" + std::to_string(dt) +
                ", moveDist=" + std::to_string(maxMoveDist));
        }
        
        // Handle vertical movement / gravity
        bool isOnGround = (player.posZ <= GROUND_LEVEL);
        
        if (isOnGround) {
            // On ground: can jump
            if (player.isJumpPressed) {
                player.velZ = JUMP_VELOCITY;
                req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) + " jumped");
            } else {
                // Stay on ground
                player.velZ = 0.0f;
            }
        } else {
            // In air: apply gravity
            player.velZ += GRAVITY * dt;
        }
        
        // Integrate position
        float newPosX = player.posX + player.velX * dt;
        float newPosY = player.posY + player.velY * dt;
        float newPosZ = player.posZ + player.velZ * dt;
        
        // Clamp to ground level
        if (newPosZ <= GROUND_LEVEL) {
            newPosZ = GROUND_LEVEL;
            player.velZ = 0.0f;
        }
        
        // Basic anti-cheat / sanity check
        float dx = newPosX - player.lastValidPosX;
        float dy = newPosY - player.lastValidPosY;
        float dz = newPosZ - player.lastValidPosZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        
        float maxAllowedMove = moveSpeed * dt * MAX_ALLOWED_MOVE_MULTIPLIER;
        float suspiciousThreshold = maxAllowedMove * SUSPICIOUS_MOVE_MULTIPLIER;
        
        if (dist > suspiciousThreshold) {
            // Clearly insane movement - snap back
            req::shared::logWarn("zone", std::string{"Movement suspicious for characterId="} + 
                std::to_string(characterId) + ", dist=" + std::to_string(dist) + 
                " (max allowed=" + std::to_string(maxAllowedMove) + "), snapping back to last valid position");
            
            player.posX = player.lastValidPosX;
            player.posY = player.lastValidPosY;
            player.posZ = player.lastValidPosZ;
            player.velX = 0.0f;
            player.velY = 0.0f;
            player.velZ = 0.0f;
        } else {
            // Accept new position
            player.posX = newPosX;
            player.posY = newPosY;
            player.posZ = newPosZ;
            
            // Update last valid position
            player.lastValidPosX = newPosX;
            player.lastValidPosY = newPosY;
            player.lastValidPosZ = newPosZ;
            
            // Mark as dirty if position changed
            if (dist > 0.01f) {  // Small threshold to avoid marking on tiny movements
                player.isDirty = true;
            }
            
            // Log position change
            if (doDetailedLog) {
                req::shared::logInfo("zone", std::string{"[Sim] Player "} + std::to_string(characterId) +
                    " AFTER: pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")" +
                    ", moved=" + std::to_string(dist) + " units");
            }
        }
    }
    
    // Update NPCs (placeholder for now - no AI yet)
    for (auto& [npcId, npc] : npcs_) {
        updateNpc(npc, dt);
    }
    
    // Periodic NPC debug logging (every 5 seconds at 20Hz = 100 ticks)
    static std::uint64_t npcLogCounter = 0;
    if (!npcs_.empty() && ++npcLogCounter % 100 == 0) {
        req::shared::logInfo("zone", std::string{"[NPC] Tick: "} + std::to_string(npcs_.size()) +
            " NPC(s) in zone (no AI yet)");
    }
}

void ZoneServer::broadcastSnapshots() {
    if (players_.empty()) {
        // No players to broadcast
        return;
    }
    
    // Log snapshot building (periodic, not every tick to reduce spam)
    static std::uint64_t logCounter = 0;
    bool doDetailedLog = (++logCounter % 20 == 0);  // Log every 20 snapshots (~1 second at 20Hz)
    
    if (doDetailedLog) {
        req::shared::logInfo("zone", std::string{"[Snapshot] Building snapshot "} + 
            std::to_string(snapshotCounter_ + 1) + " for " + std::to_string(players_.size()) + " active player(s)");
    }
    
    ++snapshotCounter_;
    
    // If broadcastFullState is true, use old behavior (single snapshot for all clients)
    if (zoneConfig_.broadcastFullState) {
        // Build single snapshot with ALL players
        req::shared::protocol::PlayerStateSnapshotData snapshot;
        snapshot.snapshotId = snapshotCounter_;
        
        for (const auto& [characterId, player] : players_) {
            if (!player.isInitialized) {
                continue;
            }
            
            req::shared::protocol::PlayerStateEntry entry;
            entry.characterId = player.characterId;
            entry.posX = player.posX;
            entry.posY = player.posY;
            entry.posZ = player.posZ;
            entry.velX = player.velX;
            entry.velY = player.velY;
            entry.velZ = player.velZ;
            entry.yawDegrees = player.yawDegrees;
            
            snapshot.players.push_back(entry);
            
            // Log what we're putting into the snapshot
            if (doDetailedLog) {
                req::shared::logInfo("zone", std::string{"[Snapshot] Adding entry: charId="} +
                    std::to_string(player.characterId) + 
                    ", pos=(" + std::to_string(entry.posX) + "," + std::to_string(entry.posY) + "," + std::to_string(entry.posZ) + ")" +
                    ", vel=(" + std::to_string(entry.velX) + "," + std::to_string(entry.velY) + "," + std::to_string(entry.velZ) + ")");
            }
        }
        
        // Build payload once
        std::string payloadStr = req::shared::protocol::buildPlayerStateSnapshotPayload(snapshot);
        req::shared::net::Connection::ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
        
        // Log the actual payload string
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Snapshot] Payload: '"} + payloadStr + "'");
        }
        
        // Broadcast to all connected clients (with safety checks)
        int sentCount = 0;
        int failedCount = 0;
        for (auto& connection : connections_) {
            if (!connection) {
                failedCount++;
                continue;
            }
            
            // Check if connection is closed
            if (connection->isClosed()) {
                failedCount++;
                continue;
            }
            
            try {
                connection->send(req::shared::MessageType::PlayerStateSnapshot, payloadBytes);
                sentCount++;
            } catch (const std::exception& e) {
                req::shared::logWarn("zone", std::string{"[Snapshot] Failed to send to connection: "} + e.what());
                failedCount++;
            }
        }
        
        // Log broadcast summary (periodic)
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Snapshot] Broadcast snapshot "} + 
                std::to_string(snapshot.snapshotId) + " with " + std::to_string(snapshot.players.size()) + 
                " player(s) to " + std::to_string(sentCount) + " connection(s) [FULL BROADCAST]" +
                (failedCount > 0 ? " (failed: " + std::to_string(failedCount) + ")" : ""));
        }
    } else {
        // Interest-based filtering: build per-recipient snapshots
        int totalSent = 0;
        int totalFailed = 0;
        
        for (auto& [recipientCharId, recipientPlayer] : players_) {
            if (!recipientPlayer.isInitialized || !recipientPlayer.connection) {
                continue;
            }
            
            // Check if connection is still valid
            if (recipientPlayer.connection->isClosed()) {
                continue;
            }
            
            // Build snapshot for this specific recipient
            req::shared::protocol::PlayerStateSnapshotData snapshot;
            snapshot.snapshotId = snapshotCounter_;
            
            float recipientX = recipientPlayer.posX;
            float recipientY = recipientPlayer.posY;
            float recipientZ = recipientPlayer.posZ;
            
            int includedCount = 0;
            
            for (const auto& [otherCharId, otherPlayer] : players_) {
                if (!otherPlayer.isInitialized) {
                    continue;
                }
                
                // Always include self
                if (otherCharId == recipientCharId) {
                    req::shared::protocol::PlayerStateEntry entry;
                    entry.characterId = otherPlayer.characterId;
                    entry.posX = otherPlayer.posX;
                    entry.posY = otherPlayer.posY;
                    entry.posZ = otherPlayer.posZ;
                    entry.velX = otherPlayer.velX;
                    entry.velY = otherPlayer.velY;
                    entry.velZ = otherPlayer.velZ;
                    entry.yawDegrees = otherPlayer.yawDegrees;
                    snapshot.players.push_back(entry);
                    includedCount++;
                    
                    // Log self entry
                    if (doDetailedLog) {
                        req::shared::logInfo("zone", std::string{"[Snapshot] For charId="} + std::to_string(recipientCharId) +
                            " adding SELF: pos=(" + std::to_string(entry.posX) + "," + std::to_string(entry.posY) + "," + std::to_string(entry.posZ) + ")");
                    }
                    continue;
                }
                
                // Check distance (2D or 3D - using 2D for now: XY plane)
                float dx = otherPlayer.posX - recipientX;
                float dy = otherPlayer.posY - recipientY;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                if (distance <= zoneConfig_.interestRadius) {
                    req::shared::protocol::PlayerStateEntry entry;
                    entry.characterId = otherPlayer.characterId;
                    entry.posX = otherPlayer.posX;
                    entry.posY = otherPlayer.posY;
                    entry.posZ = otherPlayer.posZ;
                    entry.velX = otherPlayer.velX;
                    entry.velY = otherPlayer.velY;
                    entry.velZ = otherPlayer.velZ;
                    entry.yawDegrees = otherPlayer.yawDegrees;
                    snapshot.players.push_back(entry);
                    includedCount++;
                }
            }
            
            // Debug logging (if enabled)
            if (zoneConfig_.debugInterest && doDetailedLog) {
                req::shared::logInfo("zone", std::string{"[Snapshot] (filtered) recipientCharId="} + 
                    std::to_string(recipientCharId) + ", playersIncluded=" + std::to_string(includedCount) +
                    " (out of " + std::to_string(players_.size()) + " total)");
            }
            
            // Build payload and send to this recipient (with error handling)
            try {
                std::string payloadStr = req::shared::protocol::buildPlayerStateSnapshotPayload(snapshot);
                req::shared::net::Connection::ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
                
                // Log payload for this recipient
                if (doDetailedLog) {
                    req::shared::logInfo("zone", std::string{"[Snapshot] For charId="} + std::to_string(recipientCharId) +
                        " payload: '" + payloadStr + "'");
                }

                recipientPlayer.connection->send(req::shared::MessageType::PlayerStateSnapshot, payloadBytes);
                totalSent++;
            } catch (const std::exception& e) {
                req::shared::logWarn("zone", std::string{"[Snapshot] Failed to send to charId="} + 
                    std::to_string(recipientCharId) + ": " + e.what());
                totalFailed++;
            }
        }
        
        // Log overall result (periodic)
        if (doDetailedLog) {
            req::shared::logInfo("zone", std::string{"[Snapshot] Finished sending filtered snapshots: "} +
                std::to_string(totalSent) + " sent, " + std::to_string(totalFailed) + " failed");
        }
    }
}

} // namespace req::zone

