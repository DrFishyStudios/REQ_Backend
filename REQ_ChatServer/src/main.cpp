#include <exception>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Config.h"

#include "../include/req/chat/ChatServer.h"

int main() {
    using req::shared::logError;
    using req::shared::logInfo;
    using req::shared::initLogger;

    initLogger("REQ_ChatServer");

    try {
        // Hardcoded address/port for now
        std::string address = "0.0.0.0";
        std::uint16_t port = 8201;

        req::chat::ChatServer server(address, port);
        logInfo("chat", std::string{"Starting ChatServer on "} + address + ":" + std::to_string(port));
        server.run();
    } catch (const std::exception& ex) {
        logError("chat", std::string{"Unhandled exception: "} + ex.what());
        return 1;
    } catch (...) {
        logError("chat", "Unhandled non-standard exception");
        return 1;
    }

    return 0;
}
