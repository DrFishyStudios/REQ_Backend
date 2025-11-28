#include "../include/req/shared/Connection.h"

#include <array>

namespace req::shared::net {

Connection::Connection(Tcp::socket socket)
    : socket_(std::move(socket)) {
}

void Connection::setMessageHandler(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void Connection::start() {
    req::shared::logInfo("net", "Connection started");
    doReadHeader();
}

void Connection::doReadHeader() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(&incomingHeader_, sizeof(incomingHeader_)),
        [self](boost::system::error_code ec, std::size_t bytes) {
            if (ec) {
                req::shared::logError("net", std::string{"Read header error: "} + ec.message());
                self->close();
                return;
            }
            if (bytes != sizeof(req::shared::MessageHeader)) {
                req::shared::logWarn("net", "Partial header read; closing connection");
                self->close();
                return;
            }

            if (self->incomingHeader_.payloadSize > 0) {
                self->incomingBody_.resize(self->incomingHeader_.payloadSize);
                self->doReadBody();
            } else {
                if (self->onMessage_) {
                    self->onMessage_(self->incomingHeader_, self->incomingBody_, self);
                } else {
                    req::shared::logWarn("net", "Message received but no handler installed (empty body)");
                }
                self->doReadHeader();
            }
        }
    );
}

void Connection::doReadBody() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(self->incomingBody_.data(), self->incomingBody_.size()),
        [self](boost::system::error_code ec, std::size_t bytes) {
            if (ec) {
                req::shared::logError("net", std::string{"Read body error: "} + ec.message());
                self->close();
                return;
            }
            if (bytes != self->incomingBody_.size()) {
                req::shared::logWarn("net", "Partial body read; closing connection");
                self->close();
                return;
            }

            if (self->onMessage_) {
                self->onMessage_(self->incomingHeader_, self->incomingBody_, self);
            } else {
                req::shared::logWarn("net", "Message received but no handler installed (body)");
            }
            self->doReadHeader();
        }
    );
}

void Connection::send(req::shared::MessageType type, const ByteArray& payload, std::uint64_t reserved) {
    OutgoingMessage msg;
    msg.header.type = type;
    msg.header.payloadSize = static_cast<std::uint32_t>(payload.size());
    msg.header.reserved = reserved;
    msg.body = payload;

    writeQueue_.push_back(std::move(msg));
    if (!writeInProgress_) {
        doWrite();
    }
}

void Connection::doWrite() {
    if (writeQueue_.empty()) {
        writeInProgress_ = false;
        return;
    }

    writeInProgress_ = true;
    auto& front = writeQueue_.front();

    std::array<boost::asio::const_buffer, 2> buffers = {
        boost::asio::buffer(&front.header, sizeof(front.header)),
        boost::asio::buffer(front.body.data(), front.body.size())
    };

    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        buffers,
        [self](boost::system::error_code ec, std::size_t /*bytes*/) {
            if (ec) {
                req::shared::logError("net", std::string{"Write error: "} + ec.message());
                self->close();
                return;
            }
            self->writeQueue_.pop_front();
            if (!self->writeQueue_.empty()) {
                self->doWrite();
            } else {
                self->writeInProgress_ = false;
            }
        }
    );
}

void Connection::close() {
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    req::shared::logInfo("net", "Connection closed");
}

} // namespace req::shared::net
