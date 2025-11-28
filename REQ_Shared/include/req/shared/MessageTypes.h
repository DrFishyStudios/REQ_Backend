#pragma once

#include <cstdint>

namespace req::shared {

enum class MessageType : std::uint16_t {
    // Generic / debug
    Ping = 0,   // Client or server ping
    Pong = 1,   // Response to Ping

    // Login server handshake/auth
    LoginRequest      = 10, // Client requests login with credentials
    LoginResponse     = 11, // Server responds with success/fail + token (TODO)

    // World server authentication / selection
    WorldAuthRequest  = 20, // Client presents session to world server
    WorldAuthResponse = 21, // World server validates session

    // Zone server handoff/authentication
    ZoneAuthRequest   = 30, // Client requests entry to zone with handoff token
    ZoneAuthResponse  = 31, // Zone server confirms access

    // Gameplay (initial placeholders)
    PlayerState       = 100, // Snapshot of player state (TODO payload format)
    NpcSpawn          = 101, // NPC spawn info (TODO payload format)
    ChatMessage       = 102, // Chat channel or direct message (TODO payload format)
};

// TODO: Reserve ranges per subsystem, define payload structures, document versioning.

} // namespace req::shared
