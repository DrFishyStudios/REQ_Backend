#pragma once

#include <cstdint>
#include "MessageTypes.h"

namespace req::shared {

struct MessageHeader {
    MessageType  type{ MessageType::Ping };
    std::uint32_t payloadSize{ 0 }; // size in bytes of payload that follows
    std::uint64_t reserved{ 0 };    // reserved for future use (session/routing)
};

// Basic sanity check. Layout portability TODO.
static_assert(sizeof(MessageHeader) >= 16, "MessageHeader should be at least 16 bytes.");
// TODO: Define explicit serialization (endianness, packing) before cross-platform/network release.

} // namespace req::shared
