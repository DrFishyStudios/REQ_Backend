#include <exception>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../include/req/login/LoginServer.h"

int main() {
    try {
        req::shared::initLogger("REQ_LoginServer");
        auto config = req::shared::loadLoginConfig("config/login_config.json");
        req::login::LoginServer server(config);
        server.run();
    } catch (const std::exception& ex) {
        req::shared::logError("Main", std::string("Exception in main: ") + ex.what());
        return 1;
    } catch (...) {
        req::shared::logError("Main", "Unknown exception in main.");
        return 1;
    }
    return 0;
}
