#include <exception>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../include/req/zone/ZoneServer.h"

int main() {
    try {
        req::shared::initLogger("REQ_ZoneServer");

        std::string address = "0.0.0.0";
        std::uint16_t port  = 7779;
        req::shared::ZoneId zoneId = 1;

        req::zone::ZoneServer server(address, port, zoneId);
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
