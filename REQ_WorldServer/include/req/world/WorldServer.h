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

namespace req::world {

class WorldServer {
public:
    explicit WorldServer(const req::shared::WorldConfig& config);

    void run();
    void stop();

private:
    using Tcp = boost::asio::ip::tcp;
    using ConnectionPtr = std::shared_ptr<req::shared::net::Connection>;    

    void startAccept();
    void handleNewConnection(Tcp::socket socket);

    void handleMessage(const req::shared::MessageHeader& header,
                       const req::shared::net::Connection::ByteArray& payload,
                       ConnectionPtr connection);

    req::shared::HandoffToken generateHandoffToken();

    boost::asio::io_context ioContext_{};
    Tcp::acceptor           acceptor_;

    std::vector<ConnectionPtr> connections_;
    std::unordered_map<req::shared::HandoffToken, req::shared::SessionToken> handoffs_;

    req::shared::WorldConfig config_{};
};

} // namespace req::world
