#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstdint>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
#include "../../REQ_Shared/include/req/shared/Connection.h"
#include "../../REQ_Shared/include/req/shared/Config.h"

namespace req::zone {

class ZoneServer {
public:
    ZoneServer(std::uint32_t worldId,
               std::uint32_t zoneId,
               const std::string& zoneName,
               const std::string& address,
               std::uint16_t port);

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

    boost::asio::io_context  ioContext_{};
    Tcp::acceptor            acceptor_;
    std::vector<ConnectionPtr> connections_{};

    std::uint32_t            worldId_{};
    std::uint32_t            zoneId_{};
    std::string              zoneName_{};
    std::string              address_{};
    std::uint16_t            port_{};
};

} // namespace req::zone
