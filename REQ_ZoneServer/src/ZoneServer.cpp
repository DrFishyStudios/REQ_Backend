#include "../include/req/zone/ZoneServer.h"

#include <cmath>
#include <algorithm>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

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
    
    // Log ZoneServer construction
    req::shared::logInfo("zone", std::string{"ZoneServer constructed:"});
    req::shared::logInfo("zone", std::string{"  worldId="} + std::to_string(worldId_));
    req::shared::logInfo("zone", std::string{"  zoneId="} + std::to_string(zoneId_));
    req::shared::logInfo("zone", std::string{"  zoneName=\""} + zoneName_ + "\"");
    req::shared::logInfo("zone", std::string{"  address="} + address_);
    req::shared::logInfo("zone", std::string{"  port="} + std::to_string(port_));
    req::shared::logInfo("zone", std::string{"  charactersPath="} + charactersPath);
    req::shared::logInfo("zone", std::string{"  tickRate="} + std::to_string(TICK_RATE_HZ) + " Hz");
}

void ZoneServer::run() {
    req::shared::logInfo("zone", std::string{"ZoneServer starting: worldId="} + 
        std::to_string(worldId_) + ", zoneId=" + std::to_string(zoneId_) + 
        ", zoneName=\"" + zoneName_ + "\", address=" + address_ + 
        ", port=" + std::to_string(port_));
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

    connection->setMessageHandler([this](const req::shared::MessageHeader& header,
                                         const req::shared::net::Connection::ByteArray& payload,
                                         std::shared_ptr<req::shared::net::Connection> conn) {
        handleMessage(header, payload, conn);
    });

    req::shared::logInfo("zone", std::string{"New client connected to zone \""} + zoneName_ + 
        "\" (id=" + std::to_string(zoneId_) + ")");
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
        
        ZonePlayer player;
        player.characterId = characterId;
        player.accountId = character->accountId;
        
        // Determine spawn position using character data
        spawnPlayer(*character, player);
        
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
        
        // Insert into players map
        players_[characterId] = player;
        connectionToCharacterId_[connection] = characterId;
        
        req::shared::logInfo("zone", std::string{"[ZONEAUTH] ZonePlayer initialized: characterId="} + 
            std::to_string(characterId) + ", spawn=(" + 
            std::to_string(player.posX) + "," + std::to_string(player.posY) + "," + 
            std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));

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
                std::to_string(intent.characterId));
            return;
        }
        
        ZonePlayer& player = it->second;
        
        // Validate sequence number (ignore old/duplicate packets)
        if (intent.sequenceNumber <= player.lastSequenceNumber) {
            req::shared::logInfo("zone", std::string{"MovementIntent: out-of-order or duplicate, seq="} + 
                std::to_string(intent.sequenceNumber) + " <= last=" + std::to_string(player.lastSequenceNumber) +
                " for characterId=" + std::to_string(intent.characterId));
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
        
        // Log at info level (can be disabled in production)
        // req::shared::logInfo("zone", std::string{"MovementIntent: characterId="} + 
        //     std::to_string(intent.characterId) + ", seq=" + std::to_string(intent.sequenceNumber) +
        //     ", input=(" + std::to_string(player.inputX) + "," + std::to_string(player.inputY) + ")" +
        //     ", yaw=" + std::to_string(player.yawDegrees) + ", jump=" + (player.isJumpPressed ? "1" : "0"));
        
        break;
    }
    
    case req::shared::MessageType::PlayerStateSnapshot: {
        // Server -> Client only; ignore if client sends it
        req::shared::logWarn("zone", "Received PlayerStateSnapshot from client (invalid direction)");
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
}

void ZoneServer::broadcastSnapshots() {
    if (players_.empty()) {
        // No players to broadcast
        return;
    }
    
    // Build PlayerStateSnapshotData
    req::shared::protocol::PlayerStateSnapshotData snapshot;
    snapshot.snapshotId = ++snapshotCounter_;
    
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
    
    // Build payload
    std::string payloadStr = req::shared::protocol::buildPlayerStateSnapshotPayload(snapshot);
    req::shared::net::Connection::ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
    
    // req::shared::logInfo("zone", std::string{"Broadcasting snapshot "} + std::to_string(snapshot.snapshotId) + 
    //     " with " + std::to_string(snapshot.players.size()) + " player(s)");
    
    // Broadcast to all connected clients
    for (auto& connection : connections_) {
        if (connection) {
            connection->send(req::shared::MessageType::PlayerStateSnapshot, payloadBytes);
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
        ", autosaveInterval=" + std::to_string(config.autosaveIntervalSec) + "s");
}

void ZoneServer::savePlayerPosition(std::uint64_t characterId) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        return;
    }
    
    const ZonePlayer& player = playerIt->second;
    
    // Load character from disk
    auto character = characterStore_.loadById(characterId);
    if (!character.has_value()) {
        req::shared::logWarn("zone", std::string{"[SAVE] Cannot save position - character not found: characterId="} +
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
    
    // Save to disk
    if (characterStore_.saveCharacter(*character)) {
        req::shared::logInfo("zone", std::string{"[SAVE] Position saved: characterId="} +
            std::to_string(characterId) + ", zoneId=" + std::to_string(zoneId_) +
            ", pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," +
            std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));
        
        // Mark as clean after successful save
        playerIt->second.isDirty = false;
    } else {
        req::shared::logError("zone", std::string{"[SAVE] Failed to save character position: characterId="} +
            std::to_string(characterId));
    }
}

void ZoneServer::saveAllPlayerPositions() {
    int savedCount = 0;
    int skippedCount = 0;
    
    for (auto& [characterId, player] : players_) {
        if (!player.isInitialized || !player.isDirty) {
            skippedCount++;
            continue;
        }
        
        savePlayerPosition(characterId);
        savedCount++;
    }
    
    if (savedCount > 0) {
        req::shared::logInfo("zone", std::string{"[AUTOSAVE] Saved positions for "} +
            std::to_string(savedCount) + " player(s), skipped " + std::to_string(skippedCount));
    }
}

void ZoneServer::removePlayer(std::uint64_t characterId) {
    auto it = players_.find(characterId);
    if (it == players_.end()) {
        return;
    }
    
    // Save final position before removing
    req::shared::logInfo("zone", std::string{"[DISCONNECT] Saving final position for characterId="} +
        std::to_string(characterId));
    savePlayerPosition(characterId);
    
    // Remove from players map
    players_.erase(it);
    
    req::shared::logInfo("zone", std::string{"[DISCONNECT] Player removed: characterId="} +
        std::to_string(characterId) + ", remaining players=" + std::to_string(players_.size()));
}

void ZoneServer::onConnectionClosed(ConnectionPtr connection) {
    // Find character ID for this connection
    auto it = connectionToCharacterId_.find(connection);
    if (it != connectionToCharacterId_.end()) {
        std::uint64_t characterId = it->second;
        removePlayer(characterId);
        connectionToCharacterId_.erase(it);
    }
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
        return;
    }
    
    // Save all dirty player positions
    saveAllPlayerPositions();
    
    // Schedule next autosave
    scheduleAutosave();
}

} // namespace req::zone
