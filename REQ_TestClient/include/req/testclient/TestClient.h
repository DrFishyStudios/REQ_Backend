#pragma once

#include <string>
#include <cstdint>

#include "../../REQ_Shared/include/req/shared/Types.h"

namespace req::testclient {

class TestClient {
public:
    void run();

private:
    bool doLogin(const std::string& username,
                 const std::string& password,
                 const std::string& clientVersion,
                 req::shared::SessionToken& outSessionToken,
                 req::shared::WorldId& outWorldId,
                 std::string& outWorldHost,
                 std::uint16_t& outWorldPort);

    bool doWorldAuth(const std::string& worldHost,
                     std::uint16_t worldPort,
                     req::shared::SessionToken sessionToken,
                     req::shared::WorldId worldId,
                     req::shared::HandoffToken& outHandoffToken,
                     req::shared::ZoneId& outZoneId,
                     std::string& outZoneHost,
                     std::uint16_t& outZonePort);

    bool doZoneAuth(const std::string& zoneHost,
                    std::uint16_t zonePort,
                    req::shared::HandoffToken handoffToken,
                    req::shared::PlayerId characterId);
};

} // namespace req::testclient
