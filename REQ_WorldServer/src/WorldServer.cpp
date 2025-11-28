#include "../include/req/world/WorldServer.h"

#include <random>
#include <limits>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
// TODO: Implement cross-platform process spawning (fork/exec on Linux/macOS)
#endif

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

namespace req::world {

WorldServer::WorldServer(const req::shared::WorldConfig& config)
    : acceptor_(ioContext_), config_(config) {
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
    } while (token == req::shared::InvalidHandoffToken || handoffs_.count(token) != 0);
    return token;
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
        handoffs_[handoff] = sessionToken;

        auto respPayload = req::shared::protocol::buildWorldAuthResponseOkPayload(
            handoff, zone.zoneId, zone.host, zone.port);
        req::shared::net::Connection::ByteArray respBytes(respPayload.begin(), respPayload.end());
        connection->send(req::shared::MessageType::WorldAuthResponse, respBytes);

        req::shared::logInfo("world", std::string{"WorldAuthResponse OK: handoffToken="} + 
            std::to_string(handoff) + ", zoneId=" + std::to_string(zone.zoneId) + 
            ", endpoint=" + zone.host + ":" + std::to_string(zone.port));
        break;
    }
    default:
        req::shared::logWarn("world", std::string{"Unsupported message type: "} + 
            std::to_string(static_cast<int>(header.type)));
        break;
    }
}

} // namespace req::world
