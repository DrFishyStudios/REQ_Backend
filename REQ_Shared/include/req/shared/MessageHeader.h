#pragma once

#include <cstdint>
#include "MessageTypes.h"

namespace req::shared {

// Current protocol version - increment when wire format changes
inline constexpr std::uint16_t CurrentProtocolVersion = 1;

struct MessageHeader {
    std::uint16_t protocolVersion{ CurrentProtocolVersion };
    MessageType   type{ MessageType::Ping };
    std::uint32_t payloadSize{ 0 }; // size in bytes of payload that follows
    std::uint64_t reserved{ 0 };    // reserved for future use (session/routing)
};

// Verify header is properly sized
static_assert(sizeof(MessageHeader) == 16, "MessageHeader should be exactly 16 bytes.");
// TODO: Define explicit serialization (endianness, packing) before cross-platform/network release.

} // namespace req::shared
