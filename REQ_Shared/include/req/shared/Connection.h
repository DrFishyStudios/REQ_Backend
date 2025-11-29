#pragma once

#include <memory>
#include <deque>
#include <vector>
#include <functional>

#include <boost/asio.hpp>

#include "MessageHeader.h"
#include "MessageTypes.h"
#include "Logger.h"

namespace req::shared::net {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Tcp       = boost::asio::ip::tcp;
    using ByteArray = std::vector<std::uint8_t>;
    using MessageHandler = std::function<void(const req::shared::MessageHeader&, const ByteArray&, std::shared_ptr<Connection>)>;
    using DisconnectHandler = std::function<void(std::shared_ptr<Connection>)>;

    explicit Connection(Tcp::socket socket);

    void start();

    void send(req::shared::MessageType type,
              const ByteArray& payload,
              std::uint64_t reserved = 0);

    void close();
    void setMessageHandler(MessageHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);
    
    // Check if connection is closed
    bool isClosed() const { return closed_; }

private:
    void doReadHeader();
    void doReadBody();
    void doWrite();
    
    // Internal close that can be called multiple times safely
    void closeInternal(const std::string& reason);

    Tcp::socket          socket_;
    req::shared::MessageHeader incomingHeader_{};
    ByteArray            incomingBody_;

    struct OutgoingMessage {
        req::shared::MessageHeader header;
        ByteArray                  body;
    };

    std::deque<OutgoingMessage> writeQueue_;
    bool                        writeInProgress_{ false };
    bool                        closed_{ false };  // Track if connection is closed

    MessageHandler              onMessage_;
    DisconnectHandler           onDisconnect_;
};

} // namespace req::shared::net
