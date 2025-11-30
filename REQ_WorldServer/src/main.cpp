#include <exception>
#include <string>
#include <thread>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/SessionService.h"
#include "../include/req/world/WorldServer.h"

namespace {
    // Helper to parse command-line arguments in format: --key=value
    bool parseArgument(const std::string& arg, const std::string& prefix, std::string& outValue) {
        if (arg.size() > prefix.size() && arg.substr(0, prefix.size()) == prefix) {
            outValue = arg.substr(prefix.size());
            return true;
        }
        return false;
    }
}

int main(int argc, char* argv[]) {
    try {
        req::shared::initLogger("REQ_WorldServer");
        
        // Default configuration file path
        std::string worldConfigPath = "config/world_config.json";
        bool cliMode = false;
        
        // Parse command-line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            std::string value;
            
            if (parseArgument(arg, "--config=", value)) {
                worldConfigPath = value;
                req::shared::logInfo("Main", std::string{"Command-line: using config file: "} + worldConfigPath);
            }
            else if (arg == "--cli") {
                cliMode = true;
                req::shared::logInfo("Main", "Command-line: CLI mode enabled");
            }
            else {
                req::shared::logWarn("Main", std::string{"Unknown command-line argument: "} + arg);
            }
        }
        
        req::shared::logInfo("Main", std::string{"Loading world configuration from: "} + worldConfigPath);
        auto config = req::shared::loadWorldConfig(worldConfigPath);

        req::shared::logInfo("Main", std::string{"Configuration loaded successfully:"});
        req::shared::logInfo("Main", std::string{"  worldId="} + std::to_string(config.worldId));
        req::shared::logInfo("Main", std::string{"  worldName="} + config.worldName);
        req::shared::logInfo("Main", std::string{"  address="} + config.address + ":" + std::to_string(config.port));
        req::shared::logInfo("Main", std::string{"  rulesetId="} + config.rulesetId);
        req::shared::logInfo("Main", std::string{"  zones="} + std::to_string(config.zones.size()));
        req::shared::logInfo("Main", std::string{"  autoLaunchZones="} + (config.autoLaunchZones ? "true" : "false"));

        // Load world rules based on ruleset_id from config
        const std::string rulesetId = config.rulesetId;
        std::string worldRulesPath = "config/world_rules_" + rulesetId + ".json";

        req::shared::logInfo("Main", std::string{"Loading world rules from: "} + worldRulesPath);
        auto worldRules = req::shared::loadWorldRules(worldRulesPath);

        // Initialize with CharacterStore path
        const std::string charactersPath = "data/characters";
        req::shared::logInfo("Main", std::string{"Using characters path: "} + charactersPath);

        // Configure SessionService with file-backed persistence
        const std::string sessionsPath = "data/sessions.json";
        req::shared::logInfo("Main", std::string{"Configuring SessionService with file: "} + sessionsPath);
        auto& sessionService = req::shared::SessionService::instance();
        sessionService.configure(sessionsPath);

        req::world::WorldServer server(config, worldRules, charactersPath);
        
        if (cliMode) {
            // Run server in background thread, CLI in foreground
            req::shared::logInfo("Main", "Starting server in background thread for CLI mode");
            std::thread serverThread([&server]() {
                server.run();
            });
            
            // Run CLI on main thread
            server.runCLI();
            
            // Wait for server thread to finish
            if (serverThread.joinable()) {
                serverThread.join();
            }
        } else {
            // Run server normally (blocks until shutdown)
            server.run();
        }
    } catch (const std::exception& ex) {
        req::shared::logError("Main", std::string("Fatal exception: ") + ex.what());
        req::shared::logError("Main", "WorldServer cannot start. Check configuration and try again.");
        return 1;
    } catch (...) {
        req::shared::logError("Main", "Unknown fatal exception occurred.");
        return 1;
    }
    return 0;
}
