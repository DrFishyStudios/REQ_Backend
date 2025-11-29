#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <memory>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "ClientStages.h"

namespace req::testclient {

class TestClient {
public:
    TestClient();
    
    void run();
    
    // Scenario execution modes
    void runHappyPathScenario();
    void runBadPasswordTest();
    void runBadSessionTokenTest();
    void runBadHandoffTokenTest();
    
    // Negative test mode: sends intentionally invalid requests to verify error handling
    void runNegativeTests();

private:
    using Tcp = boost::asio::ip::tcp;
    using IoContext = boost::asio::io_context;
    
    // Stage management
    EClientStage currentStage_;
    void transitionStage(EClientStage newStage, const std::string& context = "");
    
    // Session state
    req::shared::SessionToken sessionToken_;
    std::uint64_t accountId_;  // Stub for now, will be resolved from session
    req::shared::WorldId worldId_;
    req::shared::HandoffToken handoffToken_;
    req::shared::ZoneId zoneId_;
    std::uint64_t selectedCharacterId_;
    
    bool doLogin(const std::string& username,
                 const std::string& password,
                 const std::string& clientVersion,
                 req::shared::protocol::LoginMode mode,
                 req::shared::SessionToken& outSessionToken,
                 req::shared::WorldId& outWorldId,
                 std::string& outWorldHost,
                 std::uint16_t& outWorldPort);

    bool doCharacterList(const std::string& worldHost,
                        std::uint16_t worldPort,
                        req::shared::SessionToken sessionToken,
                        req::shared::WorldId worldId,
                        std::vector<req::shared::protocol::CharacterListEntry>& outCharacters);

    bool doCharacterCreate(const std::string& worldHost,
                          std::uint16_t worldPort,
                          req::shared::SessionToken sessionToken,
                          req::shared::WorldId worldId,
                          const std::string& name,
                          const std::string& race,
                          const std::string& characterClass,
                          req::shared::protocol::CharacterListEntry& outCharacter);

    bool doEnterWorld(const std::string& worldHost,
                     std::uint16_t worldPort,
                     req::shared::SessionToken sessionToken,
                     req::shared::WorldId worldId,
                     std::uint64_t characterId,
                     req::shared::HandoffToken& outHandoffToken,
                     req::shared::ZoneId& outZoneId,
                     std::string& outZoneHost,
                     std::uint16_t& outZonePort);

    bool doZoneAuthAndConnect(const std::string& zoneHost,
                             std::uint16_t zonePort,
                             req::shared::HandoffToken handoffToken,
                             req::shared::PlayerId characterId,
                             std::shared_ptr<IoContext>& outIoContext,
                             std::shared_ptr<Tcp::socket>& outSocket);
    
    void runMovementTestLoop(std::shared_ptr<IoContext> ioContext,
                            std::shared_ptr<Tcp::socket> zoneSocket,
                            std::uint64_t localCharacterId);
    
    // Negative test helpers
    bool testInvalidZoneAuth(const std::string& zoneHost, std::uint16_t zonePort);
    bool testMalformedZoneAuth(const std::string& zoneHost, std::uint16_t zonePort);
};

} // namespace req::testclient
