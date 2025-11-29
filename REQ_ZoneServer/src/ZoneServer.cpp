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
    
    constexpr float MAX_SPEED = 7.0f;                        // units per second
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
    req::shared::logInfo("zone", std::string{"  broadcastFullState="} + 
        (zoneConfig_.broadcastFullState ? "true" : "false"));
    req::shared::logInfo("zone", std::string{"  interestRadius="} + std::to_string(zoneConfig_.interestRadius));
}

void ZoneServer::loadNpcsForZone() {
    // Construct NPC config file path
    std::string npcConfigPath = "config/zones/zone_" + std::to_string(zoneId_) + "_npcs.json";
    
    req::shared::logInfo("zone", std::string{"[NPC] Loading NPCs from: "} + npcConfigPath);
    
    // Try to load NPC JSON file
    std::ifstream file(npcConfigPath);
    if (!file.is_open()) {
        req::shared::logInfo("zone", std::string{"[NPC] No NPC file found ("} + npcConfigPath + 
            "), zone will have no NPCs");
        return;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        if (!j.contains("npcs") || !j["npcs"].is_array()) {
            req::shared::logWarn("zone", "[NPC] NPC file does not contain 'npcs' array");
            return;
        }
        
        int loadedCount = 0;
        for (const auto& npcJson : j["npcs"]) {
            req::shared::data::ZoneNpc npc;
            
            // Required fields
            npc.npcId = npcJson.value("npc_id", static_cast<std::uint64_t>(0));
            npc.name = npcJson.value("name", std::string{"Unknown NPC"});
            npc.level = npcJson.value("level", static_cast<std::int32_t>(1));
            npc.maxHp = npcJson.value("max_hp", static_cast<std::int32_t>(100));
            npc.currentHp = npc.maxHp;  // Start at full HP
            npc.isAlive = true;
            
            // Position
            npc.posX = npcJson.value("pos_x", 0.0f);
            npc.posY = npcJson.value("pos_y", 0.0f);
            npc.posZ = npcJson.value("pos_z", 0.0f);
            npc.facingDegrees = npcJson.value("facing_degrees", 0.0f);
            
            // Store spawn point for leashing
            npc.spawnX = npc.posX;
            npc.spawnY = npc.posY;
            npc.spawnZ = npc.posZ;
            
            // Optional fields with defaults
            npc.minDamage = npcJson.value("min_damage", static_cast<std::int32_t>(1));
            npc.maxDamage = npcJson.value("max_damage", static_cast<std::int32_t>(5));
            npc.aggroRadius = npcJson.value("aggro_radius", 10.0f);
            npc.leashRadius = npcJson.value("leash_radius", 50.0f);
            
            // Validate NPC ID
            if (npc.npcId == 0) {
                req::shared::logWarn("zone", "[NPC] Skipping NPC with npc_id=0 (invalid)");
                continue;
            }
            
            // Check for duplicate IDs
            if (npcs_.find(npc.npcId) != npcs_.end()) {
                req::shared::logWarn("zone", std::string{"[NPC] Duplicate npc_id="} + 
                    std::to_string(npc.npcId) + ", skipping");
                continue;
            }
            
            // Add to NPC map
            npcs_[npc.npcId] = npc;
            loadedCount++;
            
            req::shared::logInfo("zone", std::string{"[NPC] Loaded: id="} + std::to_string(npc.npcId) +
                ", name=\"" + npc.name + "\", level=" + std::to_string(npc.level) +
                ", maxHp=" + std::to_string(npc.maxHp) +
                ", pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," +
                std::to_string(npc.posZ) + "), facing=" + std::to_string(npc.facingDegrees));
        }
        
        req::shared::logInfo("zone", std::string{"[NPC] Loaded "} + std::to_string(loadedCount) +
            " NPC(s) for zone " + std::to_string(zoneId_));
        
    } catch (const nlohmann::json::exception& e) {
        req::shared::logError("zone", std::string{"[NPC] JSON parse error: "} + e.what());
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[NPC] Error loading NPCs: "} + e.what());
    }
}

void ZoneServer::updateNpc(req::shared::data::ZoneNpc& npc, float deltaSeconds) {
    // Placeholder for future AI/behavior
    // For now, NPCs are stationary
    (void)npc;
    (void)deltaSeconds;
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

void ZoneServer::startAccept() {
    using boost::asio::ip::tcp;
    auto socket = std::make_shared<tcp::socket>(ioContext_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            handleNewConnection(std::move(*socket));
        } else {
            req::shared::logError("zone", std::string{"accept error: "} + ec.message());
        }
        startAccept();
    });
}

void ZoneServer::handleNewConnection(Tcp::socket socket) {
    auto connection = std::make_shared<req::shared::net::Connection>(std::move(socket));
    connections_.push_back(connection);

    connection->setMessageHandler([this, connection](const req::shared::MessageHeader& header,
                                         const req::shared::net::Connection::ByteArray& payload,
                                         std::shared_ptr<req::shared::net::Connection> conn) {
        handleMessage(header, payload, conn);
    });
    
    connection->setDisconnectHandler([this](std::shared_ptr<req::shared::net::Connection> conn) {
        onConnectionClosed(conn);
    });

    req::shared::logInfo("zone", std::string{"New client connected to zone \""} + zoneName_ + 
        "\" (id=" + std::to_string(zoneId_) + "), total connections=" + std::to_string(connections_.size()));
    connection->start();
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
        req::shared::protocol::MovementIntentData intent;
        
        if (!req::shared::protocol::parseMovementIntentPayload(body, intent)) {
            req::shared::logError("zone", "Failed to parse MovementIntent payload");
            return;
        }
        
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
    // Update player physics
    for (auto& [characterId, player] : players_) {
        if (!player.isInitialized) {
            continue;
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
        
        // Compute desired velocity (no acceleration for now, just direct)
        player.velX = dirX * MAX_SPEED;
        player.velY = dirY * MAX_SPEED;
        
        // Handle vertical movement / gravity
        bool isOnGround = (player.posZ <= GROUND_LEVEL);
        
        if (isOnGround) {
            // On ground: can jump
            if (player.isJumpPressed) {
                player.velZ = JUMP_VELOCITY;
                // req::shared::logInfo("zone", std::string{"Character "} + std::to_string(characterId) + " jumped");
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
        
        float maxAllowedMove = MAX_SPEED * dt * MAX_ALLOWED_MOVE_MULTIPLIER;
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
    if (++logCounter % 100 == 0) {  // Log every 100 snapshots (~5 seconds at 20Hz)
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
        }
        
        // Build payload once
        std::string payloadStr = req::shared::protocol::buildPlayerStateSnapshotPayload(snapshot);
        req::shared::net::Connection::ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
        
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
        if (logCounter % 100 == 0) {
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
            if (zoneConfig_.debugInterest && logCounter % 20 == 0) {  // Every 1 second at 20Hz
                req::shared::logInfo("zone", std::string{"[Snapshot] (filtered) recipientCharId="} + 
                    std::to_string(recipientCharId) + ", playersIncluded=" + std::to_string(includedCount) +
                    " (out of " + std::to_string(players_.size()) + " total)");
            }
            
            // Build payload and send to this recipient (with error handling)
            try {
                std::string payloadStr = req::shared::protocol::buildPlayerStateSnapshotPayload(snapshot);
                req::shared::net::Connection::ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
                
                recipientPlayer.connection->send(req::shared::MessageType::PlayerStateSnapshot, payloadBytes);
                totalSent++;
            } catch (const std::exception& e) {
                req::shared::logWarn("zone", std::string{"[Snapshot] Failed to send to characterId="} +
                    std::to_string(recipientCharId) + ": " + e.what());
                totalFailed++;
            }
        }
        
        // Log broadcast summary (periodic)
        if (logCounter % 100 == 0) {
            req::shared::logInfo("zone", std::string{"[Snapshot] Broadcast snapshot "} + 
                std::to_string(snapshotCounter_) + " with INTEREST FILTERING to " + 
                std::to_string(totalSent) + " client(s) [radius=" + 
                std::to_string(zoneConfig_.interestRadius) + "]" +
                (totalFailed > 0 ? " (failed: " + std::to_string(totalFailed) + ")" : ""));
        }
    }
}

void ZoneServer::spawnPlayer(req::shared::data::Character& character, ZonePlayer& player) {
    // Determine spawn position based on character's last known location
    bool useRestoredPosition = false;
    
    // Rule 1: If lastZoneId matches this zone AND position is valid (non-zero), restore position
    if (character.lastZoneId == zoneId_) {
        // Check if position is valid (not all zeros)
        bool hasValidPosition = (character.positionX != 0.0f || 
                                character.positionY != 0.0f || 
                                character.positionZ != 0.0f);
        
        if (hasValidPosition) {
            // Restore saved position
            player.posX = character.positionX;
            player.posY = character.positionY;
            player.posZ = character.positionZ;
            player.yawDegrees = character.heading;
            useRestoredPosition = true;
            
            req::shared::logInfo("zone", std::string{"[SPAWN] Restored position for characterId="} +
                std::to_string(character.characterId) + ": pos=(" +
                std::to_string(player.posX) + "," + std::to_string(player.posY) + "," +
                std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));
        }
    }
    
    // Rule 2: Otherwise, use zone safe spawn point
    if (!useRestoredPosition) {
        player.posX = zoneConfig_.safeX;
        player.posY = zoneConfig_.safeY;
        player.posZ = zoneConfig_.safeZ;
        player.yawDegrees = zoneConfig_.safeYaw;
        
        req::shared::logInfo("zone", std::string{"[SPAWN] Using safe spawn point for characterId="} +
            std::to_string(character.characterId) + " (first visit or zone mismatch): pos=(" +
            std::to_string(player.posX) + "," + std::to_string(player.posY) + "," +
            std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));
        
        // Update character record to reflect new zone and position
        character.lastWorldId = worldId_;
        character.lastZoneId = zoneId_;
        character.positionX = player.posX;
        character.positionY = player.posY;
        character.positionZ = player.posZ;
        character.heading = player.yawDegrees;
        
        // Save updated character data
        if (characterStore_.saveCharacter(character)) {
            req::shared::logInfo("zone", std::string{"[SPAWN] Updated character lastZone/position: characterId="} +
                std::to_string(character.characterId) + ", lastZoneId=" + std::to_string(zoneId_));
        } else {
            req::shared::logWarn("zone", std::string{"[SPAWN] Failed to save character position: characterId="} +
                std::to_string(character.characterId));
        }
    }
    
    // Initialize velocity
    player.velX = 0.0f;
    player.velY = 0.0f;
    player.velZ = 0.0f;
}

void ZoneServer::setZoneConfig(const ZoneConfig& config) {
    zoneConfig_ = config;
    req::shared::logInfo("zone", std::string{"Zone config updated: safeSpawn=("} +
        std::to_string(config.safeX) + "," + std::to_string(config.safeY) + "," +
        std::to_string(config.safeZ) + "), safeYaw=" + std::to_string(config.safeYaw) +
        ", autosaveInterval=" + std::to_string(config.autosaveIntervalSec) + "s" +
        ", broadcastFullState=" + (config.broadcastFullState ? "true" : "false") +
        ", interestRadius=" + std::to_string(config.interestRadius) +
        ", debugInterest=" + (config.debugInterest ? "true" : "false"));
}

void ZoneServer::savePlayerPosition(std::uint64_t characterId) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        req::shared::logWarn("zone", std::string{"[SAVE] Player not found in map: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    const ZonePlayer& player = playerIt->second;
    
    // Wrap entire save operation in try-catch
    try {
        // Load character from disk
        auto character = characterStore_.loadById(characterId);
        if (!character.has_value()) {
            req::shared::logWarn("zone", std::string{"[SAVE] Cannot save position - character not found on disk: characterId="} +
                std::to_string(characterId));
            return;
        }
        
        // Update position fields
        character->lastWorldId = worldId_;
        character->lastZoneId = zoneId_;
        character->positionX = player.posX;
        character->positionY = player.posY;
        character->positionZ = player.posZ;
        character->heading = player.yawDegrees;
        
        // Update combat state if dirty
        if (player.combatStatsDirty) {
            character->level = player.level;
            character->hp = player.hp;
            character->maxHp = player.maxHp;
            character->mana = player.mana;
            character->maxMana = player.maxMana;
            
            character->strength = player.strength;
            character->stamina = player.stamina;
            character->agility = player.agility;
            character->dexterity = player.dexterity;
            character->intelligence = player.intelligence;
            character->wisdom = player.wisdom;
            character->charisma = player.charisma;
            
            req::shared::logInfo("zone", std::string{"[SAVE] Combat stats saved: characterId="} +
                std::to_string(characterId) + ", hp=" + std::to_string(player.hp) + "/" +
                std::to_string(player.maxHp) + ", mana=" + std::to_string(player.mana) + "/" +
                std::to_string(player.maxMana));
        }
        
        // Save to disk
        if (characterStore_.saveCharacter(*character)) {
            req::shared::logInfo("zone", std::string{"[SAVE] Position saved successfully: characterId="} +
                std::to_string(characterId) + ", zoneId=" + std::to_string(zoneId_) +
                ", pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," +
                std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));
            
            // Mark as clean after successful save
            playerIt->second.isDirty = false;
            playerIt->second.combatStatsDirty = false;
        } else {
            req::shared::logError("zone", std::string{"[SAVE] Failed to save character to disk: characterId="} +
                std::to_string(characterId));
        }
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[SAVE] Exception during save: characterId="} +
            std::to_string(characterId) + ", error: " + e.what());
        // Don't rethrow - just log and return
    } catch (...) {
        req::shared::logError("zone", std::string{"[SAVE] Unknown exception during save: characterId="} +
            std::to_string(characterId));
        // Don't rethrow - just log and return
    }
}

void ZoneServer::saveAllPlayerPositions() {
    int savedCount = 0;
    int skippedCount = 0;
    int failedCount = 0;
    
    req::shared::logInfo("zone", "[AUTOSAVE] Beginning autosave of dirty player positions");
    
    for (auto& [characterId, player] : players_) {
        if (!player.isInitialized) {
            skippedCount++;
            continue;
        }
        
        if (!player.isDirty && !player.combatStatsDirty) {
            skippedCount++;
            continue;
        }
        
        try {
            savePlayerPosition(characterId);
            savedCount++;
        } catch (const std::exception& e) {
            req::shared::logError("zone", std::string{"[AUTOSAVE] Exception saving characterId="} +
                std::to_string(characterId) + ": " + e.what());
            failedCount++;
        } catch (...) {
            req::shared::logError("zone", std::string{"[AUTOSAVE] Unknown exception saving characterId="} +
                std::to_string(characterId));
            failedCount++;
        }
    }
    
    if (savedCount > 0 || failedCount > 0) {
        req::shared::logInfo("zone", std::string{"[AUTOSAVE] Complete: saved="} +
            std::to_string(savedCount) + ", skipped=" + std::to_string(skippedCount) +
            ", failed=" + std::to_string(failedCount));
    }
}

void ZoneServer::removePlayer(std::uint64_t characterId) {
    req::shared::logInfo("zone", std::string{"[REMOVE_PLAYER] BEGIN: characterId="} + 
        std::to_string(characterId));
    
    auto it = players_.find(characterId);
    if (it == players_.end()) {
        req::shared::logWarn("zone", std::string{"[REMOVE_PLAYER] Character not found in players map: characterId="} +
            std::to_string(characterId));
        req::shared::logInfo("zone", "[REMOVE_PLAYER] END (player not found)");
        return;
    }
    
    const ZonePlayer& player = it->second;
    
    req::shared::logInfo("zone", std::string{"[REMOVE_PLAYER] Found player: accountId="} +
        std::to_string(player.accountId) + ", pos=(" + std::to_string(player.posX) + "," +
        std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")");
    
    // Save final position before removing (wrapped in try-catch)
    req::shared::logInfo("zone", "[REMOVE_PLAYER] Attempting to save character state...");
    try {
        savePlayerPosition(characterId);
        req::shared::logInfo("zone", "[REMOVE_PLAYER] Character state saved successfully");
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[REMOVE_PLAYER] Exception during savePlayerPosition: "} + 
            e.what());
        req::shared::logError("zone", "[REMOVE_PLAYER] Continuing with player removal despite save failure");
    } catch (...) {
        req::shared::logError("zone", "[REMOVE_PLAYER] Unknown exception during savePlayerPosition");
        req::shared::logError("zone", "[REMOVE_PLAYER] Continuing with player removal despite save failure");
    }
    
    // Remove from connection mapping (if connection still exists)
    if (player.connection) {
        auto connIt = connectionToCharacterId_.find(player.connection);
        if (connIt != connectionToCharacterId_.end()) {
            connectionToCharacterId_.erase(connIt);
            req::shared::logInfo("zone", "[REMOVE_PLAYER] Removed from connection mapping");
        }
    }
    
    // Remove from players map
    players_.erase(it);
    req::shared::logInfo("zone", "[REMOVE_PLAYER] Removed from players map");
    
    req::shared::logInfo("zone", std::string{"[REMOVE_PLAYER] END: characterId="} +
        std::to_string(characterId) + ", remaining_players=" + std::to_string(players_.size()));
}

void ZoneServer::onConnectionClosed(ConnectionPtr connection) {
    req::shared::logInfo("zone", "[DISCONNECT] ========== BEGIN DISCONNECT HANDLING ==========");
    req::shared::logInfo("zone", "[DISCONNECT] Connection closed event received");
    
    // Get remote endpoint info for logging (if still available)
    try {
        if (connection && connection->isClosed()) {
            req::shared::logInfo("zone", "[DISCONNECT] Connection is marked as closed");
        }
    } catch (const std::exception& e) {
        req::shared::logWarn("zone", std::string{"[DISCONNECT] Error checking connection state: "} + e.what());
    }
    
    // Find character ID for this connection
    auto it = connectionToCharacterId_.find(connection);
    if (it != connectionToCharacterId_.end()) {
        std::uint64_t characterId = it->second;
        req::shared::logInfo("zone", std::string{"[DISCONNECT] Found ZonePlayer: characterId="} + 
            std::to_string(characterId));
        
        // Check if player exists
        auto playerIt = players_.find(characterId);
        if (playerIt != players_.end()) {
            req::shared::logInfo("zone", std::string{"[DISCONNECT] Player found in players map, accountId="} +
                std::to_string(playerIt->second.accountId) + 
                ", pos=(" + std::to_string(playerIt->second.posX) + "," + 
                std::to_string(playerIt->second.posY) + "," + 
                std::to_string(playerIt->second.posZ) + ")");
        } else {
            req::shared::logWarn("zone", std::string{"[DISCONNECT] CharacterId "} + 
                std::to_string(characterId) + " found in connection map but not in players map (inconsistent state)");
        }
        
        // Remove player (with safe save)
        removePlayer(characterId);
        
        // Remove from connection mapping
        connectionToCharacterId_.erase(it);
        req::shared::logInfo("zone", "[DISCONNECT] Removed from connection-to-character mapping");
    } else {
        req::shared::logInfo("zone", "[DISCONNECT] No ZonePlayer associated with this connection");
        req::shared::logInfo("zone", "[DISCONNECT] Likely disconnected before completing ZoneAuthRequest");
    }
    
    // Remove from connections list
    auto connIt = std::find(connections_.begin(), connections_.end(), connection);
    if (connIt != connections_.end()) {
        connections_.erase(connIt);
        req::shared::logInfo("zone", "[DISCONNECT] Removed from connections list");
    }
    
    req::shared::logInfo("zone", std::string{"[DISCONNECT] Cleanup complete. Active connections="} +
        std::to_string(connections_.size()) + ", active players=" + std::to_string(players_.size()));
    req::shared::logInfo("zone", "[DISCONNECT] ========== END DISCONNECT HANDLING ==========");
}

void ZoneServer::scheduleAutosave() {
    auto intervalMs = std::chrono::milliseconds(
        static_cast<int>(zoneConfig_.autosaveIntervalSec * 1000.0f));
    
    autosaveTimer_.expires_after(intervalMs);
    autosaveTimer_.async_wait([this](const boost::system::error_code& ec) {
        onAutosave(ec);
    });
}

void ZoneServer::onAutosave(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
        req::shared::logInfo("zone", "Autosave timer cancelled (server shutting down)");
        return;
    }
    
    if (ec) {
        req::shared::logError("zone", std::string{"Autosave timer error: "} + ec.message());
        // Continue with next autosave anyway
        scheduleAutosave();
        return;
    }
    
    // Save all dirty player positions (wrapped in try-catch)
    try {
        saveAllPlayerPositions();
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[AUTOSAVE] Exception during autosave: "} + e.what());
    } catch (...) {
        req::shared::logError("zone", "[AUTOSAVE] Unknown exception during autosave");
    }
    
    // Always schedule next autosave, even if this one failed
    scheduleAutosave();
}

void ZoneServer::processAttack(ZonePlayer& attacker, req::shared::data::ZoneNpc& target,
                               const req::shared::protocol::AttackRequestData& request) {
    // Check if target is already dead
    if (!target.isAlive || target.currentHp <= 0) {
        req::shared::protocol::AttackResultData result;
        result.attackerId = attacker.characterId;
        result.targetId = target.npcId;
        result.damage = 0;
        result.wasHit = false;
        result.remainingHp = 0;
        result.resultCode = 5; // Target is dead
        result.message = target.name + " is already dead";
        
        broadcastAttackResult(result);
        return;
    }
    
    // Check range (simple Euclidean distance)
    float dx = attacker.posX - target.posX;
    float dy = attacker.posY - target.posY;
    float dz = attacker.posZ - target.posZ;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    
    constexpr float MAX_ATTACK_RANGE = 200.0f; // Configurable
    
    if (distance > MAX_ATTACK_RANGE) {
        req::shared::logWarn("zone", std::string{"[COMBAT] Out of range: distance="} +
            std::to_string(distance) + ", max=" + std::to_string(MAX_ATTACK_RANGE));
        
        req::shared::protocol::AttackResultData result;
        result.attackerId = attacker.characterId;
        result.targetId = target.npcId;
        result.damage = 0;
        result.wasHit = false;
        result.remainingHp = target.currentHp;
        result.resultCode = 1; // Out of range
        result.message = "Target out of range";
        
        broadcastAttackResult(result);
        return;
    }
    
    // Calculate hit chance (simple: 95% hit for now)
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> hitRoll(1, 100);
    bool didHit = (hitRoll(rng) <= 95);
    
    if (!didHit) {
        req::shared::logInfo("zone", std::string{"[COMBAT] Attack missed: attacker="} +
            std::to_string(attacker.characterId) + ", target=" + std::to_string(target.npcId));
        
        req::shared::protocol::AttackResultData result;
        result.attackerId = attacker.characterId;
        result.targetId = target.npcId;
        result.damage = 0;
        result.wasHit = false;
        result.remainingHp = target.currentHp;
        result.resultCode = 0; // Success (but missed)
        result.message = "You miss " + target.name;
        
        broadcastAttackResult(result);
        return;
    }
    
    // Calculate damage: base damage from level + strength bonus + random
    int baseDamage = 5 + (attacker.level * 2);
    int strengthBonus = attacker.strength / 10;
    std::uniform_int_distribution<int> damageVariance(-2, 5);
    int variance = damageVariance(rng);
    
    int totalDamage = baseDamage + strengthBonus + variance;
    totalDamage = std::max(1, totalDamage); // Minimum 1 damage
    
    // Apply damage to NPC
    target.currentHp -= totalDamage;
    bool targetDied = false;
    
    if (target.currentHp <= 0) {
        target.currentHp = 0;
        target.isAlive = false;
        targetDied = true;
        
        req::shared::logInfo("zone", std::string{"[COMBAT] NPC slain: npcId="} +
            std::to_string(target.npcId) + ", name=\"" + target.name + 
            "\", killerCharId=" + std::to_string(attacker.characterId));
    }
    
    // Build result message
    std::ostringstream msgBuilder;
    if (targetDied) {
        msgBuilder << "You hit " << target.name << " for " << totalDamage 
                   << " points of damage. " << target.name << " has been slain!";
    } else {
        msgBuilder << "You hit " << target.name << " for " << totalDamage << " points of damage";
    }
    
    req::shared::logInfo("zone", std::string{"[COMBAT] Attack hit: attacker="} +
        std::to_string(attacker.characterId) + ", target=" + std::to_string(target.npcId) +
        ", damage=" + std::to_string(totalDamage) + ", remainingHp=" + std::to_string(target.currentHp));
    
    // Build and broadcast result
    req::shared::protocol::AttackResultData result;
    result.attackerId = attacker.characterId;
    result.targetId = target.npcId;
    result.damage = totalDamage;
    result.wasHit = true;
    result.remainingHp = target.currentHp;
    result.resultCode = 0; // Success
    result.message = msgBuilder.str();
    
    broadcastAttackResult(result);
}

void ZoneServer::broadcastAttackResult(const req::shared::protocol::AttackResultData& result) {
    std::string payload = req::shared::protocol::buildAttackResultPayload(result);
    req::shared::net::Connection::ByteArray payloadBytes(payload.begin(), payload.end());
    
    req::shared::logInfo("zone", std::string{"[COMBAT] AttackResult: attacker="} +
        std::to_string(result.attackerId) + ", target=" + std::to_string(result.targetId) +
        ", dmg=" + std::to_string(result.damage) + ", hit=" + (result.wasHit ? "1" : "0") +
        ", remainingHp=" + std::to_string(result.remainingHp) +
        ", resultCode=" + std::to_string(result.resultCode) +
        ", msg=\"" + result.message + "\"");
    
    // Broadcast to all clients in the zone (for now - could optimize to nearby only)
    int sentCount = 0;
    for (auto& connection : connections_) {
        if (connection) {
            connection->send(req::shared::MessageType::AttackResult, payloadBytes);
            sentCount++;
        }
    }
    
    req::shared::logInfo("zone", std::string{"[COMBAT] Broadcast AttackResult to "} +
        std::to_string(sentCount) + " client(s)");
}

} // namespace req::zone
