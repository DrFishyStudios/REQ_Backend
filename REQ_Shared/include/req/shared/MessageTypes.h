#pragma once

#include <cstdint>

namespace req::shared {

enum class MessageType : std::uint16_t {
    // Generic / debug
    Ping = 0,   // Client or server ping
    Pong = 1,   // Response to Ping

    // Login server handshake/auth
    LoginRequest      = 10, // Client requests login with credentials
    LoginResponse     = 11, // Server responds with success/fail + token

    // World server authentication / selection
    WorldAuthRequest  = 20, // Client presents session to world server
    WorldAuthResponse = 21, // World server validates session
    
    // Character management
    CharacterListRequest   = 22, // Client requests character list for account/world
    CharacterListResponse  = 23, // World server responds with character list
    CharacterCreateRequest = 24, // Client requests character creation
    CharacterCreateResponse = 25, // World server responds with created character
    EnterWorldRequest      = 26, // Client requests to enter world with character
    EnterWorldResponse     = 27, // World server responds with zone handoff

    // Zone server handoff/authentication
    ZoneAuthRequest   = 30, // Client requests entry to zone with handoff token
    ZoneAuthResponse  = 31, // Zone server confirms access
    
    // Zone gameplay - Movement (server-authoritative model)
    MovementIntent        = 40, // Client sends movement input to ZoneServer
    PlayerStateSnapshot   = 41, // ZoneServer sends authoritative player states to client

    // Gameplay (initial placeholders)
    PlayerState       = 100, // Snapshot of player state (DEPRECATED - use PlayerStateSnapshot)
    NpcSpawn          = 101, // NPC spawn info (TODO payload format)
    ChatMessage       = 102, // Chat channel or direct message (TODO payload format)
};

// TODO: Reserve ranges per subsystem, define payload structures, document versioning.

} // namespace req::shared
