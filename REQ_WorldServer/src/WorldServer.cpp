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

} // namespace req::world
