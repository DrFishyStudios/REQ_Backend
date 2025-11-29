#pragma once

#include <string>

namespace req::testclient {

/*
 * Client Handshake Stages
 * 
 * Represents the current state of the TestClient during the handshake process.
 * Used for state tracking, logging, and error handling.
 */
enum class EClientStage {
    NotConnected,       // Initial state, not connected to any server
    LoginPending,       // Sent LoginRequest, awaiting response
    LoggedIn,           // LoginResponse OK received, have sessionToken
    WorldSelected,      // World chosen, connecting to WorldServer
    CharactersLoaded,   // CharacterListResponse received
    EnteringWorld,      // EnterWorldRequest sent, awaiting zone handoff
    InZone,             // ZoneAuthResponse OK, in zone and ready for gameplay
    Error               // Error occurred, handshake failed
};

/*
 * Convert stage enum to string for logging
 */
inline std::string stageToString(EClientStage stage) {
    switch (stage) {
        case EClientStage::NotConnected:     return "NotConnected";
        case EClientStage::LoginPending:     return "LoginPending";
        case EClientStage::LoggedIn:         return "LoggedIn";
        case EClientStage::WorldSelected:    return "WorldSelected";
        case EClientStage::CharactersLoaded: return "CharactersLoaded";
        case EClientStage::EnteringWorld:    return "EnteringWorld";
        case EClientStage::InZone:           return "InZone";
        case EClientStage::Error:            return "Error";
        default:                             return "Unknown";
    }
}

} // namespace req::testclient
