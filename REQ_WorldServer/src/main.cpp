#include <exception>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
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
        
        // Parse command-line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            std::string value;
            
            if (parseArgument(arg, "--config=", value)) {
                worldConfigPath = value;
                req::shared::logInfo("Main", std::string{"Command-line: using config file: "} + worldConfigPath);
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

        req::world::WorldServer server(config);
        server.run();
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
