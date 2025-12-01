#include "../include/req/world/WorldServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/SessionService.h"

#include <string>
#include <random>
#include <limits>

namespace req::world {

req::shared::HandoffToken WorldServer::generateHandoffToken() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<req::shared::HandoffToken> dist(1, std::numeric_limits<req::shared::HandoffToken>::max());
    req::shared::HandoffToken token = 0;
    do {
        token = dist(rng);
    } while (token == req::shared::InvalidHandoffToken || handoffTokenToCharacterId_.count(token) != 0);
    return token;
}

std::optional<std::uint64_t> WorldServer::resolveSessionToken(req::shared::SessionToken token) const {
    auto& sessionService = req::shared::SessionService::instance();
    
    // First attempt: validate session from in-memory cache
    auto session = sessionService.validateSession(token);
    
    if (!session.has_value()) {
        // Session not found in memory - try reloading from file
        // This handles the case where LoginServer just wrote a new session
        req::shared::logInfo("world", std::string{"Session not in memory, reloading from file: sessionToken="} + 
            std::to_string(token));
        
        // Cast away const temporarily to call non-const loadFromFile
        // This is safe since we're only modifying the SessionService singleton, not this WorldServer
        const_cast<req::shared::SessionService&>(sessionService).loadFromFile();
        
        // Second attempt: validate session after reload
        session = sessionService.validateSession(token);
        
        if (session.has_value()) {
            req::shared::logInfo("world", std::string{"Session found after reload: sessionToken="} + 
                std::to_string(token) + ", accountId=" + std::to_string(session->accountId));
        }
    }
    
    if (session.has_value()) {
        return session->accountId;
    }
    
    return std::nullopt;
}

void WorldServer::handleMessage(const req::shared::MessageHeader& header,
                                const req::shared::net::Connection::ByteArray& payload,
                                ConnectionPtr connection) {
    // Log protocol version
    req::shared::logInfo("world", std::string{"Received message: type="} + 
        std::to_string(static_cast<int>(header.type)) + 
        ", protocolVersion=" + std::to_string(header.protocolVersion) +
        ", payloadSize=" + std::to_string(header.payloadSize));

    // Check protocol version
    if (header.protocolVersion != req::shared::CurrentProtocolVersion) {
        req::shared::logWarn("world", std::string{"Protocol version mismatch: client="} + 
            std::to_string(header.protocolVersion) + ", server=" + std::to_string(req::shared::CurrentProtocolVersion));
    }

    std::string body(payload.begin(), payload.end());

    switch (header.type) {
    case req::shared::MessageType::WorldAuthRequest: {
        req::shared::SessionToken sessionToken = 0;
        req::shared::WorldId worldId = 0;
        
        if (!req::shared::protocol::parseWorldAuthRequestPayload(body, sessionToken, worldId)) {
            req::shared::logError("world", "Failed to parse WorldAuthRequest payload");
            auto errPayload = req::shared::protocol::buildWorldAuthResponseErrorPayload("PARSE_ERROR", "Malformed world auth request");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::WorldAuthResponse, errBytes);
            return;
        }

        req::shared::logInfo("world", std::string{"WorldAuthRequest: sessionToken="} + 
            std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId));

        // Validate worldId matches this server
        if (worldId != config_.worldId) {
            req::shared::logWarn("world", std::string{"WorldId mismatch: requested="} + 
                std::to_string(worldId) + ", server=" + std::to_string(config_.worldId));
            auto errPayload = req::shared::protocol::buildWorldAuthResponseErrorPayload("WRONG_WORLD", 
                "This world server does not match requested worldId");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::WorldAuthResponse, errBytes);
            return;
        }

        // Check we have zones configured
        if (config_.zones.empty()) {
            req::shared::logError("world", "No zones configured for this world");
            auto errPayload = req::shared::protocol::buildWorldAuthResponseErrorPayload("NO_ZONES", 
                "No zones available on this world server");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::WorldAuthResponse, errBytes);
            return;
        }

        // TODO: Validate sessionToken with login server or shared session service
        // For now, accept any non-zero token
        if (sessionToken == req::shared::InvalidSessionToken) {
            req::shared::logWarn("world", "Invalid session token");
            auto errPayload = req::shared::protocol::buildWorldAuthResponseErrorPayload("INVALID_SESSION", 
                "Session token not recognized");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::WorldAuthResponse, errBytes);
            return;
        }

        // Select first available zone (TODO: load balancing, player's last zone, etc.)
        const auto& zone = config_.zones[0];
        auto handoff = generateHandoffToken();
        handoffTokenToCharacterId_[handoff] = 0; // No character yet in old flow

        auto respPayload = req::shared::protocol::buildWorldAuthResponseOkPayload(
            handoff, zone.zoneId, zone.host, zone.port);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        connection->send(req::shared::MessageType::WorldAuthResponse, respBytes);

        req::shared::logInfo("world", std::string{"WorldAuthResponse OK: handoffToken="} + 
            std::to_string(handoff) + ", zoneId=" + std::to_string(zone.zoneId) + 
            ", endpoint=" + zone.host + ":" + std::to_string(zone.port));
        break;
    }
    
    case req::shared::MessageType::CharacterListRequest: {
        req::shared::SessionToken sessionToken = 0;
        req::shared::WorldId worldId = 0;
        
        if (!req::shared::protocol::parseCharacterListRequestPayload(body, sessionToken, worldId)) {
            req::shared::logError("world", "Failed to parse CharacterListRequest payload");
            auto errPayload = req::shared::protocol::buildCharacterListResponseErrorPayload("PARSE_ERROR", "Malformed character list request");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::CharacterListResponse, errBytes);
            return;
        }

        req::shared::logInfo("world", std::string{"CharacterListRequest: sessionToken="} + 
            std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId));

        // Validate worldId
        if (worldId != config_.worldId) {
            req::shared::logWarn("world", std::string{"WorldId mismatch: requested="} + 
                std::to_string(worldId) + ", server=" + std::to_string(config_.worldId));
            auto errPayload = req::shared::protocol::buildCharacterListResponseErrorPayload("WRONG_WORLD", 
                "This world server does not match requested worldId");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::CharacterListResponse, errBytes);
            return;
        }

        // Resolve sessionToken to accountId
        auto accountId = resolveSessionToken(sessionToken);
        if (!accountId.has_value()) {
            req::shared::logWarn("world", "Invalid session token");
            auto errPayload = req::shared::protocol::buildCharacterListResponseErrorPayload("INVALID_SESSION", 
                "Session token not recognized");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::CharacterListResponse, errBytes);
            return;
        }

        // Load characters for this account and world
        auto characters = characterStore_.loadCharactersForAccountAndWorld(*accountId, worldId);
        
        req::shared::logInfo("world", std::string{"CharacterListRequest: accountId="} + 
            std::to_string(*accountId) + ", worldId=" + std::to_string(worldId) + 
            ", characters found=" + std::to_string(characters.size()));

        // Build response with character entries
        std::vector<req::shared::protocol::CharacterListEntry> entries;
        for (const auto& ch : characters) {
            req::shared::protocol::CharacterListEntry entry;
            entry.characterId = ch.characterId;
            entry.name = ch.name;
            entry.race = ch.race;
            entry.characterClass = ch.characterClass;
            entry.level = ch.level;
            entries.push_back(entry);
            
            req::shared::logInfo("world", std::string{"  Character: id="} + std::to_string(ch.characterId) + 
                ", name=" + ch.name + ", race=" + ch.race + ", class=" + ch.characterClass + 
                ", level=" + std::to_string(ch.level));
        }

        auto respPayload = req::shared::protocol::buildCharacterListResponseOkPayload(entries);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        connection->send(req::shared::MessageType::CharacterListResponse, respBytes);
        break;
    }
    
    case req::shared::MessageType::CharacterCreateRequest: {
        req::shared::SessionToken sessionToken = 0;
        req::shared::WorldId worldId = 0;
        std::string name, race, characterClass;
        
        if (!req::shared::protocol::parseCharacterCreateRequestPayload(body, sessionToken, worldId, name, race, characterClass)) {
            req::shared::logError("world", "Failed to parse CharacterCreateRequest payload");
            auto errPayload = req::shared::protocol::buildCharacterCreateResponseErrorPayload("PARSE_ERROR", "Malformed character create request");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::CharacterCreateResponse, errBytes);
            return;
        }

        req::shared::logInfo("world", std::string{"CharacterCreateRequest: sessionToken="} + 
            std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId) + 
            ", name=" + name + ", race=" + race + ", class=" + characterClass);

        // Validate worldId
        if (worldId != config_.worldId) {
            req::shared::logWarn("world", std::string{"WorldId mismatch: requested="} + 
                std::to_string(worldId) + ", server=" + std::to_string(config_.worldId));
            auto errPayload = req::shared::protocol::buildCharacterCreateResponseErrorPayload("WRONG_WORLD", 
                "This world server does not match requested worldId");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::CharacterCreateResponse, errBytes);
            return;
        }

        // Resolve sessionToken to accountId
        auto accountId = resolveSessionToken(sessionToken);
        if (!accountId.has_value()) {
            req::shared::logWarn("world", "Invalid session token");
            auto errPayload = req::shared::protocol::buildCharacterCreateResponseErrorPayload("INVALID_SESSION", 
                "Session token not recognized");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::CharacterCreateResponse, errBytes);
            return;
        }

        // Attempt to create character
        try {
            auto newCharacter = characterStore_.createCharacterForAccount(*accountId, worldId, name, race, characterClass);
            
            req::shared::logInfo("world", std::string{"Character created successfully: id="} + 
                std::to_string(newCharacter.characterId) + ", accountId=" + std::to_string(*accountId) + 
                ", name=" + name + ", race=" + race + ", class=" + characterClass);

            auto respPayload = req::shared::protocol::buildCharacterCreateResponseOkPayload(
                newCharacter.characterId, newCharacter.name, newCharacter.race, 
                newCharacter.characterClass, newCharacter.level);
            req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
            connection->send(req::shared::MessageType::CharacterCreateResponse, respBytes);
        } catch (const std::exception& e) {
            std::string errorMsg = e.what();
            req::shared::logWarn("world", std::string{"Character creation failed: "} + errorMsg);
            
            // Determine error code based on exception message
            std::string errorCode = "CREATE_FAILED";
            if (errorMsg.find("already exists") != std::string::npos || errorMsg.find("name") != std::string::npos) {
                errorCode = "NAME_TAKEN";
            } else if (errorMsg.find("invalid race") != std::string::npos) {
                errorCode = "INVALID_RACE";
            } else if (errorMsg.find("invalid class") != std::string::npos) {
                errorCode = "INVALID_CLASS";
            }
            
            auto errPayload = req::shared::protocol::buildCharacterCreateResponseErrorPayload(errorCode, errorMsg);
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::CharacterCreateResponse, errBytes);
        }
        break;
    }
    
    case req::shared::MessageType::EnterWorldRequest: {
        req::shared::SessionToken sessionToken = 0;
        req::shared::WorldId worldId = 0;
        std::uint64_t characterId = 0;
        
        if (!req::shared::protocol::parseEnterWorldRequestPayload(body, sessionToken, worldId, characterId)) {
            req::shared::logError("world", "Failed to parse EnterWorldRequest payload");
            auto errPayload = req::shared::protocol::buildEnterWorldResponseErrorPayload("PARSE_ERROR", "Malformed enter world request");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::EnterWorldResponse, errBytes);
            return;
        }

        req::shared::logInfo("world", std::string{"EnterWorldRequest: sessionToken="} + 
            std::to_string(sessionToken) + ", worldId=" + std::to_string(worldId) + 
            ", characterId=" + std::to_string(characterId));

        // Validate worldId
        if (worldId != config_.worldId) {
            req::shared::logWarn("world", std::string{"WorldId mismatch: requested="} + 
                std::to_string(worldId) + ", server=" + std::to_string(config_.worldId));
            auto errPayload = req::shared::protocol::buildEnterWorldResponseErrorPayload("WRONG_WORLD", 
                "This world server does not match requested worldId");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::EnterWorldResponse, errBytes);
            return;
        }

        // Resolve sessionToken to accountId
        auto accountId = resolveSessionToken(sessionToken);
        if (!accountId.has_value()) {
            req::shared::logWarn("world", "Invalid session token");
            auto errPayload = req::shared::protocol::buildEnterWorldResponseErrorPayload("INVALID_SESSION", 
                "Session token not recognized");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::EnterWorldResponse, errBytes);
            return;
        }

        // Load character
        auto character = characterStore_.loadById(characterId);
        if (!character.has_value()) {
            req::shared::logWarn("world", std::string{"Character not found: id="} + std::to_string(characterId));
            auto errPayload = req::shared::protocol::buildEnterWorldResponseErrorPayload("CHARACTER_NOT_FOUND", 
                "Character does not exist");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::EnterWorldResponse, errBytes);
            return;
        }

        // Verify character belongs to this account
        if (character->accountId != *accountId) {
            req::shared::logWarn("world", std::string{"Character ownership mismatch: characterId="} + 
                std::to_string(characterId) + ", expected accountId=" + std::to_string(*accountId) + 
                ", actual accountId=" + std::to_string(character->accountId));
            auto errPayload = req::shared::protocol::buildEnterWorldResponseErrorPayload("ACCESS_DENIED", 
                "Character does not belong to this account");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::EnterWorldResponse, errBytes);
            return;
        }

        // Verify character is for this world
        if (character->homeWorldId != worldId && character->lastWorldId != worldId) {
            req::shared::logWarn("world", std::string{"Character world mismatch: characterId="} + 
                std::to_string(characterId) + ", homeWorldId=" + std::to_string(character->homeWorldId) + 
                ", lastWorldId=" + std::to_string(character->lastWorldId) + ", requested=" + std::to_string(worldId));
            auto errPayload = req::shared::protocol::buildEnterWorldResponseErrorPayload("WRONG_WORLD_CHARACTER", 
                "Character does not belong to this world");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::EnterWorldResponse, errBytes);
            return;
        }

        // Determine which zone to place character in
        req::shared::ZoneId targetZoneId = character->lastZoneId;
        if (targetZoneId == 0) {
            // Use default starting zone (East Freeport)
            targetZoneId = 10;
            req::shared::logInfo("world", std::string{"Character has no last zone, using default zone "} + std::to_string(targetZoneId));
        }

        // Find zone in config
        const req::shared::WorldZoneConfig* targetZone = nullptr;
        for (const auto& zone : config_.zones) {
            if (zone.zoneId == targetZoneId) {
                targetZone = &zone;
                break;
            }
        }

        // Fallback to first zone if target not found
        if (!targetZone && !config_.zones.empty()) {
            req::shared::logWarn("world", std::string{"Target zone "} + std::to_string(targetZoneId) + 
                " not found, using first available zone");
            targetZone = &config_.zones[0];
            targetZoneId = targetZone->zoneId;
        }

        if (!targetZone) {
            req::shared::logError("world", "No zones configured");
            auto errPayload = req::shared::protocol::buildEnterWorldResponseErrorPayload("NO_ZONES", 
                "No zones available");
            req::shared::net::Connection::ByteArray errBytes(errPayload.begin(), errPayload.end());
            connection->send(req::shared::MessageType::EnterWorldResponse, errBytes);
            return;
        }

        // Generate handoff token
        auto handoff = generateHandoffToken();
        handoffTokenToCharacterId_[handoff] = characterId;

        // Bind session to this world
        auto& sessionService = req::shared::SessionService::instance();
        sessionService.bindSessionToWorld(sessionToken, static_cast<std::int32_t>(config_.worldId));

        auto respPayload = req::shared::protocol::buildEnterWorldResponseOkPayload(
            handoff, targetZoneId, targetZone->host, targetZone->port);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        connection->send(req::shared::MessageType::EnterWorldResponse, respBytes);

        req::shared::logInfo("world", std::string{"EnterWorldResponse OK: characterId="} + 
            std::to_string(characterId) + ", characterName=" + character->name + 
            ", handoffToken=" + std::to_string(handoff) + 
            ", zoneId=" + std::to_string(targetZoneId) + 
            ", endpoint=" + targetZone->host + ":" + std::to_string(targetZone->port));
        break;
    }
    
    default:
        req::shared::logWarn("world", std::string{"Unsupported message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        break;
    }
}

} // namespace req::world
