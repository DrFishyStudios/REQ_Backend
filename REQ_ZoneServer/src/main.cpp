#include <exception>
#include <string>
#include <cstdlib>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../include/req/zone/ZoneServer.h"

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
        req::shared::initLogger("REQ_ZoneServer");
        
        // Default values
        std::uint32_t worldId = 1;
        std::uint32_t zoneId = 1;
        std::string zoneName = "UnknownZone";
        std::string address = "0.0.0.0";
        std::uint16_t port = 7779;
        
        bool worldIdProvided = false;
        bool zoneIdProvided = false;
        bool zoneNameProvided = false;
        bool portProvided = false;
        
        // Parse command-line arguments
        req::shared::logInfo("Main", std::string{"Parsing "} + std::to_string(argc - 1) + " command-line argument(s)");
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            std::string value;
            
            if (parseArgument(arg, "--world_id=", value)) {
                try {
                    worldId = static_cast<std::uint32_t>(std::stoul(value));
                    worldIdProvided = true;
                    req::shared::logInfo("Main", std::string{"  Parsed --world_id="} + std::to_string(worldId));
                } catch (const std::exception& e) {
                    req::shared::logError("Main", std::string{"Failed to parse --world_id value '"} + value + "': " + e.what());
                    return 1;
                }
            }
            else if (parseArgument(arg, "--zone_id=", value)) {
                try {
                    zoneId = static_cast<std::uint32_t>(std::stoul(value));
                    zoneIdProvided = true;
                    req::shared::logInfo("Main", std::string{"  Parsed --zone_id="} + std::to_string(zoneId));
                } catch (const std::exception& e) {
                    req::shared::logError("Main", std::string{"Failed to parse --zone_id value '"} + value + "': " + e.what());
                    return 1;
                }
            }
            else if (parseArgument(arg, "--zone_name=", value)) {
                zoneName = value;
                zoneNameProvided = true;
                req::shared::logInfo("Main", std::string{"  Parsed --zone_name=\""} + zoneName + "\"");
            }
            else if (parseArgument(arg, "--port=", value)) {
                try {
                    unsigned long portValue = std::stoul(value);
                    if (portValue == 0 || portValue > 65535) {
                        req::shared::logError("Main", std::string{"Invalid port value: "} + value + " (must be 1-65535)");
                        return 1;
                    }
                    port = static_cast<std::uint16_t>(portValue);
                    portProvided = true;
                    req::shared::logInfo("Main", std::string{"  Parsed --port="} + std::to_string(port));
                } catch (const std::exception& e) {
                    req::shared::logError("Main", std::string{"Failed to parse --port value '"} + value + "': " + e.what());
                    return 1;
                }
            }
            else if (parseArgument(arg, "--address=", value)) {
                address = value;
                req::shared::logInfo("Main", std::string{"  Parsed --address="} + address);
            }
            else {
                req::shared::logWarn("Main", std::string{"Unknown command-line argument: "} + arg);
            }
        }
        
        // Log default values used
        if (!worldIdProvided) {
            req::shared::logWarn("Main", std::string{"Using DEFAULT worldId="} + std::to_string(worldId) + " (--world_id not provided)");
        }
        if (!zoneIdProvided) {
            req::shared::logWarn("Main", std::string{"Using DEFAULT zoneId="} + std::to_string(zoneId) + " (--zone_id not provided)");
        }
        if (!zoneNameProvided) {
            req::shared::logWarn("Main", std::string{"Using DEFAULT zoneName=\""} + zoneName + "\" (--zone_name not provided)");
        }
        if (!portProvided) {
            req::shared::logWarn("Main", std::string{"Using DEFAULT port="} + std::to_string(port) + " (--port not provided)");
        }
        
        // Summary of final configuration
        req::shared::logInfo("Main", "Final ZoneServer configuration:");
        req::shared::logInfo("Main", std::string{"  worldId="} + std::to_string(worldId));
        req::shared::logInfo("Main", std::string{"  zoneId="} + std::to_string(zoneId));
        req::shared::logInfo("Main", std::string{"  zoneName=\""} + zoneName + "\"");
        req::shared::logInfo("Main", std::string{"  address="} + address);
        req::shared::logInfo("Main", std::string{"  port="} + std::to_string(port));
        
        // Load world config to get ruleset ID
        req::shared::logInfo("Main", "Loading world configuration...");
        std::string worldConfigPath = "config/world_config.json";
        auto worldConfig = req::shared::loadWorldConfig(worldConfigPath);
        
        // Load world rules based on ruleset ID
        req::shared::logInfo("Main", std::string{"Loading world rules for ruleset: "} + worldConfig.rulesetId);
        std::string worldRulesPath = "config/world_rules_" + worldConfig.rulesetId + ".json";
        auto worldRules = req::shared::loadWorldRules(worldRulesPath);
        
        // Load XP table
        req::shared::logInfo("Main", "Loading XP tables...");
        std::string xpTablesPath = "config/xp_tables.json";
        auto xpTable = req::shared::loadDefaultXpTable(xpTablesPath);
        
        // Initialize ZoneServer with characters path
        std::string charactersPath = "data/characters";
        req::shared::logInfo("Main", std::string{"  charactersPath="} + charactersPath);

        req::zone::ZoneServer server(worldId, zoneId, zoneName, address, port, 
                                     worldRules, xpTable, charactersPath);
        server.run();
    } catch (const std::exception& ex) {
        req::shared::logError("Main", std::string("Fatal exception: ") + ex.what());
        req::shared::logError("Main", "ZoneServer cannot start.");
        return 1;
    } catch (...) {
        req::shared::logError("Main", "Unknown fatal exception occurred.");
        return 1;
    }
    return 0;
}
