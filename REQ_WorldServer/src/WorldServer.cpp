#include "../include/req/world/WorldServer.h"

#include <random>
#include <limits>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
// TODO: Implement cross-platform process spawning (fork/exec on Linux/macOS)
#endif

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/SessionService.h"
#include "../../REQ_Shared/include/req/shared/AccountStore.h"

namespace req::world {

WorldServer::WorldServer(const req::shared::WorldConfig& config,
                         const std::string& charactersPath)
    : acceptor_(ioContext_), config_(config), characterStore_(charactersPath),
      accountStore_("data/accounts") {
    using boost::asio::ip::tcp;
    boost::system::error_code ec;
    tcp::endpoint endpoint(boost::asio::ip::make_address(config_.address, ec), config_.port);
    if (ec) {
        req::shared::logError("world", std::string{"Invalid listen address: "} + ec.message());
        return;
    }
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) { req::shared::logError("world", std::string{"acceptor open failed: "} + ec.message()); }
    acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    if (ec) { req::shared::logError("world", std::string{"acceptor bind failed: "} + ec.message()); }
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) { req::shared::logError("world", std::string{"acceptor listen failed: "} + ec.message()); }
    
    // Log WorldServer construction details
    req::shared::logInfo("world", std::string{"WorldServer constructed:"});
    req::shared::logInfo("world", std::string{"  worldId="} + std::to_string(config_.worldId));
    req::shared::logInfo("world", std::string{"  worldName="} + config_.worldName);
    req::shared::logInfo("world", std::string{"  autoLaunchZones="} + (config_.autoLaunchZones ? "true" : "false"));
    req::shared::logInfo("world", std::string{"  zones.size()="} + std::to_string(config_.zones.size()));
    req::shared::logInfo("world", std::string{"  charactersPath="} + charactersPath);
}

void WorldServer::run() {
    req::shared::logInfo("world", std::string{"WorldServer starting: worldId="} + 
        std::to_string(config_.worldId) + ", worldName=" + config_.worldName);
    req::shared::logInfo("world", std::string{"Listening on "} + config_.address + ":" + std::to_string(config_.port));
    req::shared::logInfo("world", std::string{"Ruleset: "} + config_.rulesetId + 
        ", zones=" + std::to_string(config_.zones.size()) + 
        ", autoLaunchZones=" + (config_.autoLaunchZones ? "true" : "false"));
    
    // Handle auto-launch before entering IO loop
    if (config_.autoLaunchZones) {
        req::shared::logInfo("world", "Auto-launch is ENABLED - attempting to spawn zone processes");
        launchConfiguredZones();
    } else {
        req::shared::logInfo("world", "Auto-launch is DISABLED - zone processes expected to be managed externally");
    }
    
    startAccept();
    req::shared::logInfo("world", "Entering IO event loop...");
    ioContext_.run();
}

void WorldServer::stop() {
    req::shared::logInfo("world", "WorldServer shutdown requested");
    ioContext_.stop();
}

void WorldServer::launchConfiguredZones() {
    req::shared::logInfo("world", std::string{"launchConfiguredZones: processing "} + 
        std::to_string(config_.zones.size()) + " zone(s)");
    
    int successCount = 0;
    int failCount = 0;
    
    for (const auto& zone : config_.zones) {
        req::shared::logInfo("world", std::string{"Processing zone: id="} + 
            std::to_string(zone.zoneId) + ", name=" + zone.zoneName);
        req::shared::logInfo("world", std::string{"  endpoint="} + zone.host + ":" + std::to_string(zone.port));
        req::shared::logInfo("world", std::string{"  executable="} + 
            (zone.executablePath.empty() ? "<empty>" : zone.executablePath));
        req::shared::logInfo("world", std::string{"  args.size()="} + std::to_string(zone.args.size()));
        
        // Validation
        if (zone.executablePath.empty()) {
            req::shared::logError("world", std::string{"Zone "} + std::to_string(zone.zoneId) + 
                " (" + zone.zoneName + ") has empty executable_path - skipping");
            failCount++;
            continue;
        }
        
        if (zone.port == 0) {
            req::shared::logError("world", std::string{"Zone "} + std::to_string(zone.zoneId) + 
                " (" + zone.zoneName + ") has invalid port 0 - skipping");
            failCount++;
            continue;
        }
        
        // Attempt to spawn
        if (spawnZoneProcess(zone)) {
            req::shared::logInfo("world", std::string{"? Successfully launched zone "} + 
                std::to_string(zone.zoneId) + " (" + zone.zoneName + ")");
            successCount++;
        } else {
            req::shared::logError("world", std::string{"? Failed to launch zone "} + 
                std::to_string(zone.zoneId) + " (" + zone.zoneName + ")");
            failCount++;
        }
    }
    
    req::shared::logInfo("world", std::string{"Auto-launch summary: "} + 
        std::to_string(successCount) + " succeeded, " + 
        std::to_string(failCount) + " failed");
}

bool WorldServer::spawnZoneProcess(const req::shared::WorldZoneConfig& zone) {
#ifdef _WIN32
    // Build command line: executable + args + zone_name
    std::ostringstream cmdLineBuilder;
    
    // Quote executable path if it contains spaces
    std::string exePath = zone.executablePath;
    if (exePath.find(' ') != std::string::npos) {
        cmdLineBuilder << "\"" << exePath << "\"";
    } else {
        cmdLineBuilder << exePath;
    }
    
    // Add arguments from config
    for (const auto& arg : zone.args) {
        cmdLineBuilder << " ";
        if (arg.find(' ') != std::string::npos) {
            cmdLineBuilder << "\"" << arg << "\"";
        } else {
            cmdLineBuilder << arg;
        }
    }
    
    // Append --zone_name argument
    // Build the argument value and quote it if it contains spaces
    std::string zoneNameArg = "--zone_name=" + zone.zoneName;
    cmdLineBuilder << " ";
    if (zone.zoneName.find(' ') != std::string::npos) {
        // Quote the entire --zone_name=value if zoneName has spaces
        cmdLineBuilder << "\"" << zoneNameArg << "\"";
    } else {
        cmdLineBuilder << zoneNameArg;
    }
    
    std::string commandLine = cmdLineBuilder.str();
    req::shared::logInfo("world", std::string{"Spawning process with full command line:"});
    req::shared::logInfo("world", std::string{"  "} + commandLine);
    
    // Windows CreateProcess
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    // CreateProcessA requires a mutable command line buffer
    std::vector<char> cmdLineBuf(commandLine.begin(), commandLine.end());
    cmdLineBuf.push_back('\0');
    
    BOOL result = CreateProcessA(
        NULL,                   // Application name (NULL = use command line)
        cmdLineBuf.data(),      // Command line (mutable)
        NULL,                   // Process security attributes
        NULL,                   // Thread security attributes
        FALSE,                  // Inherit handles
        CREATE_NEW_CONSOLE,     // Creation flags (new console window for zone server)
        NULL,                   // Environment
        NULL,                   // Current directory
        &si,                    // Startup info
        &pi                     // Process info
    );
    
    if (!result) {
        DWORD error = GetLastError();
        req::shared::logError("world", std::string{"CreateProcess failed with error code: "} + 
            std::to_string(error));
        return false;
    }
    
    req::shared::logInfo("world", std::string{"Process created successfully - PID: "} + 
        std::to_string(pi.dwProcessId));
    
    // Close handles (we don't need to wait for the child process)
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
    
#else
    // TODO: Implement cross-platform process spawning
    // On Linux/macOS, use fork() + exec() or posix_spawn()
    req::shared::logWarn("world", "Auto-launch not implemented on this platform (non-Windows)");
    req::shared::logWarn("world", std::string{"Zone "} + std::to_string(zone.zoneId) + 
        " must be started manually");
    return false;
#endif
}

void WorldServer::startAccept() {
    using boost::asio::ip::tcp;
    auto socket = std::make_shared<tcp::socket>(ioContext_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        if (!ec) {
            handleNewConnection(std::move(*socket));
        } else {
            req::shared::logError("world", std::string{"accept error: "} + ec.message());
        }
        startAccept();
    });
}

void WorldServer::handleNewConnection(Tcp::socket socket) {
    auto connection = std::make_shared<req::shared::net::Connection>(std::move(socket));
    connections_.push_back(connection);

    connection->setMessageHandler([this](const req::shared::MessageHeader& header,
                                         const req::shared::net::Connection::ByteArray& payload,
                                         std::shared_ptr<req::shared::net::Connection> conn) {
        handleMessage(header, payload, conn);
    });

    req::shared::logInfo("world", "New client connected");
    connection->start();
}

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
    
    auto session = sessionService.validateSession(token);
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

void WorldServer::runCLI() {
    req::shared::logInfo("world", "");
    req::shared::logInfo("world", "=== WorldServer CLI ===");
    req::shared::logInfo("world", "Type 'help' for available commands, 'quit' to exit");
    req::shared::logInfo("world", "");
    
    std::string line;
    while (true) {
        std::cout << "\n> ";
        if (!std::getline(std::cin, line)) {
            break; // EOF or error
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);
        
        if (line.empty()) {
            continue;
        }
        
        if (line == "quit" || line == "exit" || line == "q") {
            req::shared::logInfo("world", "CLI quit requested - shutting down server");
            stop();
            break;
        }
        
        try {
            handleCLICommand(line);
        } catch (const std::exception& e) {
            req::shared::logError("world", std::string{"CLI command error: "} + e.what());
        }
    }
}

void WorldServer::handleCLICommand(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "help" || cmd == "?") {
        cmdHelp();
    } else if (cmd == "list_accounts") {
        cmdListAccounts();
    } else if (cmd == "list_chars") {
        std::uint64_t accountId = 0;
        if (!(iss >> accountId)) {
            req::shared::logError("world", "Usage: list_chars <accountId>");
            return;
        }
        cmdListChars(accountId);
    } else if (cmd == "show_char") {
        std::uint64_t characterId = 0;
        if (!(iss >> characterId)) {
            req::shared::logError("world", "Usage: show_char <characterId>");
            return;
        }
        cmdShowChar(characterId);
    } else {
        req::shared::logWarn("world", std::string{"Unknown command: '"} + cmd + "' (type 'help' for commands)");
    }
}

void WorldServer::cmdHelp() {
    std::cout << "\n=== WorldServer CLI Commands ===\n";
    std::cout << "  help, ?                  Show this help message\n";
    std::cout << "  list_accounts            List all accounts\n";
    std::cout << "  list_chars <accountId>   List all characters for an account\n";
    std::cout << "  show_char <characterId>  Show detailed character information\n";
    std::cout << "  quit, exit, q            Shutdown the server\n";
    std::cout << "===============================\n";
}

void WorldServer::cmdListAccounts() {
    auto accounts = accountStore_.loadAllAccounts();
    
    if (accounts.empty()) {
        req::shared::logInfo("world", "No accounts found");
        return;
    }
    
    req::shared::logInfo("world", std::string{"Found "} + std::to_string(accounts.size()) + " account(s):");
    
    for (const auto& account : accounts) {
        std::cout << "  id=" << account.accountId
                  << " username=" << account.username
                  << " display=\"" << account.displayName << "\""
                  << " admin=" << (account.isAdmin ? "Y" : "N")
                  << " banned=" << (account.isBanned ? "Y" : "N")
                  << "\n";
    }
}

void WorldServer::cmdListChars(std::uint64_t accountId) {
    // Verify account exists
    auto account = accountStore_.loadById(accountId);
    if (!account.has_value()) {
        req::shared::logError("world", std::string{"Account not found: id="} + std::to_string(accountId));
        return;
    }
    
    req::shared::logInfo("world", std::string{"Characters for accountId="} + std::to_string(accountId) +
        " (username=" + account->username + "):");
    
    // Load characters for all worlds (world filter removed for debugging)
    auto characters = characterStore_.loadCharactersForAccountAndWorld(accountId, config_.worldId);
    
    if (characters.empty()) {
        std::cout << "  (no characters)\n";
        return;
    }
    
    for (const auto& ch : characters) {
        std::cout << "  id=" << ch.characterId
                  << " name=" << ch.name
                  << " race=" << ch.race
                  << " class=" << ch.characterClass
                  << " lvl=" << ch.level
                  << " zone=" << ch.lastZoneId
                  << " pos=(" << ch.positionX << "," << ch.positionY << "," << ch.positionZ << ")"
                  << "\n";
    }
}

void WorldServer::cmdShowChar(std::uint64_t characterId) {
    auto character = characterStore_.loadById(characterId);
    
    if (!character.has_value()) {
        req::shared::logError("world", std::string{"Character not found: id="} + std::to_string(characterId));
        return;
    }
    
    const auto& ch = *character;
    
    std::cout << "\n=== Character Details ===\n";
    std::cout << "Character ID:     " << ch.characterId << "\n";
    std::cout << "Account ID:       " << ch.accountId << "\n";
    std::cout << "Name:             " << ch.name << "\n";
    std::cout << "Race:             " << ch.race << "\n";
    std::cout << "Class:            " << ch.characterClass << "\n";
    std::cout << "Level:            " << ch.level << "\n";
    std::cout << "XP:               " << ch.xp << "\n";
    std::cout << "\n";
    std::cout << "Home World:       " << ch.homeWorldId << "\n";
    std::cout << "Last World:       " << ch.lastWorldId << "\n";
    std::cout << "Last Zone:        " << ch.lastZoneId << "\n";
    std::cout << "\n";
    std::cout << "Position:         (" << ch.positionX << ", " << ch.positionY << ", " << ch.positionZ << ")\n";
    std::cout << "Heading:          " << ch.heading << " degrees\n";
    std::cout << "\n";
    std::cout << "Bind World:       " << ch.bindWorldId << "\n";
    std::cout << "Bind Zone:        " << ch.bindZoneId << "\n";
    std::cout << "Bind Position:    (" << ch.bindX << ", " << ch.bindY << ", " << ch.bindZ << ")\n";
    std::cout << "\n";
    std::cout << "HP:               " << ch.hp << " / " << ch.maxHp << "\n";
    std::cout << "Mana:             " << ch.mana << " / " << ch.maxMana << "\n";
    std::cout << "\n";
    std::cout << "Stats:\n";
    std::cout << "  STR: " << ch.strength << "  STA: " << ch.stamina << "\n";
    std::cout << "  AGI: " << ch.agility << "  DEX: " << ch.dexterity << "\n";
    std::cout << "  WIS: " << ch.wisdom << "  INT: " << ch.intelligence << "\n";
    std::cout << "  CHA: " << ch.charisma << "\n";
    std::cout << "=========================\n";
}

} // namespace req::world
