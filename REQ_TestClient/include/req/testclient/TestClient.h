#pragma once

#include <string>
#include <cstdint>

namespace req::testclient {

class TestClient {
public:
    void run();

private:
    bool doLogin(const std::string& username,
                 const std::string& password,
                 std::string& outWorldHost,
                 std::uint16_t& outWorldPort,
                 std::uint64_t& outSessionToken);

    bool doWorldAuth(const std::string& worldHost,
                     std::uint16_t worldPort,
                     std::uint64_t sessionToken,
                     std::string& outZoneHost,
                     std::uint16_t& outZonePort,
                     std::uint64_t& outHandoffToken);

    bool doZoneAuth(const std::string& zoneHost,
                    std::uint16_t zonePort,
                    std::uint64_t handoffToken);
};

} // namespace req::testclient
