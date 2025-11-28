#include <exception>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../include/req/login/LoginServer.h"

int main() {
    try {
        req::shared::initLogger("REQ_LoginServer");
        req::shared::logInfo("Main", "Loading configuration...");

        auto config = req::shared::loadLoginConfig("config/login_config.json");

        req::shared::logInfo("Main", std::string{"LoginConfig loaded: address="} +
                                   config.address + ", port=" + std::to_string(config.port));

        // Load world list
        req::shared::logInfo("Main", "Loading world list...");
        auto worldList = req::shared::loadWorldListConfig("config/worlds.json");

        req::shared::logInfo("Main", std::string{"WorldList loaded: "} +
                                   std::to_string(worldList.worlds.size()) + " world(s) available");

        // Initialize with AccountStore path
        const std::string accountsPath = "data/accounts";
        req::shared::logInfo("Main", std::string{"Using accounts path: "} + accountsPath);

        req::login::LoginServer server(config, worldList, accountsPath);
        server.run();
    } catch (const std::exception& ex) {
        req::shared::logError("Main", std::string("Fatal exception: ") + ex.what());
        req::shared::logError("Main", "LoginServer cannot start. Check configuration and try again.");
        return 1;
    } catch (...) {
        req::shared::logError("Main", "Unknown fatal exception occurred.");
        return 1;
    }
    return 0;
}
