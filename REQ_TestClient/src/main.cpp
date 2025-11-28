#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../include/req/testclient/TestClient.h"

int main() {
    req::shared::initLogger("REQ_TestClient");
    req::testclient::TestClient client;
    client.run();
    return 0;
}
