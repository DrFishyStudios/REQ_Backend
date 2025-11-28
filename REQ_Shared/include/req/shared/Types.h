#pragma once

#include <cstdint>

namespace req::shared {

using AccountId    = std::uint64_t;
using PlayerId     = std::uint64_t;
using WorldId      = std::uint32_t;
using ZoneId       = std::uint32_t;

using SessionToken = std::uint64_t;
using HandoffToken = std::uint64_t;

inline constexpr AccountId    InvalidAccountId    = 0; // restore for existing code
inline constexpr PlayerId     InvalidPlayerId     = 0; // restore for existing code
inline constexpr WorldId      InvalidWorldId      = 0; // restore for existing code
inline constexpr ZoneId       InvalidZoneId       = 0; // restore for existing code
inline constexpr SessionToken InvalidSessionToken = 0;
inline constexpr HandoffToken InvalidHandoffToken = 0;

using TimestampMs  = std::uint64_t;  // milliseconds since epoch (TODO: replace with chrono types later)
using Milliseconds = std::uint32_t;  // duration values

} // namespace req::shared
