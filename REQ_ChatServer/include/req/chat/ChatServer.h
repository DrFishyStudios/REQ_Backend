#pragma once

#include <string>
#include <cstdint>

#include <boost/asio.hpp>

namespace req::chat {

class ChatServer {
public:
    ChatServer(std::string address, std::uint16_t port);

    void run();
    void stop();

private:
    void startAccept(); // TODO: implement async accept later

private:
    boost::asio::io_context io_{};
    boost::asio::ip::tcp::acceptor acceptor_;

    std::string address_;
    std::uint16_t port_{};
};

} // namespace req::chat
