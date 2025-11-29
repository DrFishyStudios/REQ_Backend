#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/Connection.h"
#include "../../REQ_Shared/include/req/shared/Config.h"
#include "../../REQ_Shared/include/req/shared/CharacterStore.h"
#include "../../REQ_Shared/include/req/shared/AccountStore.h"

namespace req::world {

class WorldServer {
public:
    explicit WorldServer(const req::shared::WorldConfig& config,
                        const std::string& charactersPath = "data/characters");

    void run();
    void stop();
    
    // CLI command interface
    void runCLI();

private:
    using Tcp = boost::asio::ip::tcp;
    using ConnectionPtr = std::shared_ptr<req::shared::net::Connection>;    

    void startAccept();
    void handleNewConnection(Tcp::socket socket);

    void handleMessage(const req::shared::MessageHeader& header,
                       const req::shared::net::Connection::ByteArray& payload,
                       ConnectionPtr connection);

    req::shared::HandoffToken generateHandoffToken();
    
    // Auto-launch functionality
    void launchConfiguredZones();
    bool spawnZoneProcess(const req::shared::WorldZoneConfig& zone);
    
    // Session resolution - uses SessionService from REQ_Shared
    std::optional<std::uint64_t> resolveSessionToken(req::shared::SessionToken token) const;
    
    // CLI command handlers
    void handleCLICommand(const std::string& command);
    void cmdListAccounts();
    void cmdListChars(std::uint64_t accountId);
    void cmdShowChar(std::uint64_t characterId);
    void cmdHelp();

    boost::asio::io_context ioContext_{};
    Tcp::acceptor           acceptor_;

    std::vector<ConnectionPtr> connections_;
    std::unordered_map<req::shared::HandoffToken, std::uint64_t> handoffTokenToCharacterId_;

    req::shared::WorldConfig config_{};
    
    // Character persistence
    req::shared::CharacterStore characterStore_;
    
    // Account persistence
    req::shared::AccountStore accountStore_;
};

} // namespace req::world
