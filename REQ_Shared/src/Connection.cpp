#include "../include/req/shared/Connection.h"
#include "../include/req/shared/MessageHeader.h"

#include <array>

namespace req::shared::net {

Connection::Connection(Tcp::socket socket)
    : socket_(std::move(socket)) {
}

void Connection::setMessageHandler(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void Connection::setDisconnectHandler(DisconnectHandler handler) {
    onDisconnect_ = std::move(handler);
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
                if (ec == boost::asio::error::eof) {
                    req::shared::logInfo("net", "Connection closed by peer (EOF)");
                } else if (ec == boost::asio::error::operation_aborted) {
                    req::shared::logInfo("net", "Read operation cancelled (connection closing)");
                } else {
                    req::shared::logWarn("net", std::string{"Read header error: "} + ec.message() + 
                        " (code: " + std::to_string(ec.value()) + ")");
                }
                self->closeInternal("read header error: " + ec.message());
                return;
            }
            if (bytes != sizeof(req::shared::MessageHeader)) {
                req::shared::logWarn("net", "Partial header read; closing connection");
                self->closeInternal("partial header read");
                return;
            }

            // Check protocol version
            if (self->incomingHeader_.protocolVersion != req::shared::CurrentProtocolVersion) {
                req::shared::logWarn("net", 
                    std::string{"Protocol version mismatch: received "} + 
                    std::to_string(self->incomingHeader_.protocolVersion) +
                    ", expected " + std::to_string(req::shared::CurrentProtocolVersion));
                // TODO: In the future, enforce strict version matching and disconnect
                // For now, continue processing the message
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
                if (ec == boost::asio::error::eof) {
                    req::shared::logInfo("net", "Connection closed by peer (EOF during body read)");
                } else if (ec == boost::asio::error::operation_aborted) {
                    req::shared::logInfo("net", "Read body operation cancelled");
                } else {
                    req::shared::logWarn("net", std::string{"Read body error: "} + ec.message());
                }
                self->closeInternal("read body error: " + ec.message());
                return;
            }
            if (bytes != self->incomingBody_.size()) {
                req::shared::logWarn("net", "Partial body read; closing connection");
                self->closeInternal("partial body read");
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
    msg.header.protocolVersion = req::shared::CurrentProtocolVersion; // Set protocol version on send
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
                if (ec == boost::asio::error::eof) {
                    req::shared::logInfo("net", "Connection closed by peer during write");
                } else if (ec == boost::asio::error::operation_aborted) {
                    req::shared::logInfo("net", "Write operation cancelled");
                } else {
                    req::shared::logWarn("net", std::string{"Write error: "} + ec.message());
                }
                self->closeInternal("write error: " + ec.message());
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
    closeInternal("explicit close() call");
}

void Connection::closeInternal(const std::string& reason) {
    // Prevent double-close
    if (closed_) {
        return;
    }
    closed_ = true;
    
    req::shared::logInfo("net", std::string{"[DISCONNECT] Connection closing: reason="} + reason);
    
    // Cancel any pending async operations
    boost::system::error_code ec;
    socket_.cancel(ec);
    if (ec) {
        req::shared::logWarn("net", std::string{"Error cancelling socket operations: "} + ec.message());
    }
    
    // Shutdown socket
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec && ec != boost::asio::error::not_connected) {
        req::shared::logWarn("net", std::string{"Error shutting down socket: "} + ec.message());
    }
    
    // Close socket
    socket_.close(ec);
    if (ec) {
        req::shared::logWarn("net", std::string{"Error closing socket: "} + ec.message());
    }
    
    req::shared::logInfo("net", "[DISCONNECT] Socket closed successfully");
    
    // Notify disconnect handler if set (only once)
    if (onDisconnect_) {
        req::shared::logInfo("net", "[DISCONNECT] Notifying disconnect handler");
        try {
            auto callback = std::move(onDisconnect_);
            onDisconnect_ = nullptr; // Clear to prevent re-entry
            callback(shared_from_this());
        } catch (const std::exception& e) {
            req::shared::logError("net", std::string{"Exception in disconnect handler: "} + e.what());
        } catch (...) {
            req::shared::logError("net", "Unknown exception in disconnect handler");
        }
    }
}

} // namespace req::shared::net
