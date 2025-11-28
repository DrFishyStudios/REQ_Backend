#include "../include/req/chat/ChatServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/Connection.h"

namespace req::chat {

ChatServer::ChatServer(std::string address, std::uint16_t port)
    : acceptor_(io_), address_(std::move(address)), port_(port)
{
    using boost::asio::ip::tcp;
    tcp::endpoint endpoint(boost::asio::ip::make_address(address_), port_);
    boost::system::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
}

void ChatServer::run() {
    req::shared::logInfo("chat", "ChatServer starting");
    req::shared::logInfo("chat", "Listening on " + address_ + ":" + std::to_string(port_));
    startAccept();
    io_.run();
}

void ChatServer::stop() {
    req::shared::logInfo("chat", "ChatServer shutdown requested");
    io_.stop();
}

void ChatServer::startAccept() {
    // TODO: async_accept and create req::shared::net::Connection instances
    req::shared::logInfo("chat", "startAccept() called (stub)");
}

} // namespace req::chat
