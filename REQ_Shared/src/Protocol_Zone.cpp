#include "../include/req/shared/ProtocolSchemas.h"
#include "../include/req/shared/Logger.h"

#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace req::shared::protocol {

namespace {
    // Helper: split string by delimiter
    std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> tokens;
        std::size_t start = 0;
        while (true) {
            auto pos = s.find(delim, start);
            if (pos == std::string::npos) {
                tokens.emplace_back(s.substr(start));
                break;
            }
            tokens.emplace_back(s.substr(start, pos - start));
            start = pos + 1;
        }
        return tokens;
    }

    // Helper: parse unsigned integer safely
    template<typename T>
    bool parseUInt(const std::string& s, T& out) {
        try {
            if constexpr (sizeof(T) <= sizeof(unsigned long)) {
                out = static_cast<T>(std::stoul(s));
            } else {
                out = static_cast<T>(std::stoull(s));
            }
            return true;
        } catch (...) {
            return false;
        }
    }
}

// ============================================================================
// ZoneAuthRequest
// ============================================================================

std::string buildZoneAuthRequestPayload(
    HandoffToken handoffToken,
    PlayerId characterId) {
    std::ostringstream oss;
    oss << handoffToken << '|' << characterId;
    return oss.str();
}

bool parseZoneAuthRequestPayload(
    const std::string& payload,
    HandoffToken& outHandoffToken,
    PlayerId& outCharacterId) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        req::shared::logError("Protocol", "ZoneAuthRequest: expected 2 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    if (!parseUInt(tokens[0], outHandoffToken)) {
        req::shared::logError("Protocol", "ZoneAuthRequest: failed to parse handoffToken");
        return false;
    }
    if (!parseUInt(tokens[1], outCharacterId)) {
        req::shared::logError("Protocol", "ZoneAuthRequest: failed to parse characterId");
        return false;
    }
    return true;
}

// ============================================================================
// ZoneAuthResponse
// ============================================================================

std::string buildZoneAuthResponseOkPayload(
    const std::string& welcomeMessage) {
    std::ostringstream oss;
    oss << "OK|" << welcomeMessage;
    return oss.str();
}

std::string buildZoneAuthResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage) {
    std::ostringstream oss;
    oss << "ERR|" << errorCode << '|' << errorMessage;
    return oss.str();
}

bool parseZoneAuthResponsePayload(
    const std::string& payload,
    ZoneAuthResponseData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.empty()) {
        req::shared::logError("Protocol", "ZoneAuthResponse: empty payload");
        return false;
    }

    if (tokens[0] == "OK") {
        if (tokens.size() < 2) {
            req::shared::logError("Protocol", "ZoneAuthResponse OK: expected 2 fields");
            return false;
        }
        outData.success = true;
        outData.welcomeMessage = tokens[1];
        return true;
    } else if (tokens[0] == "ERR") {
        if (tokens.size() < 3) {
            req::shared::logError("Protocol", "ZoneAuthResponse ERR: expected 3 fields");
            return false;
        }
        outData.success = false;
        outData.errorCode = tokens[1];
        outData.errorMessage = tokens[2];
        return true;
    } else {
        req::shared::logError("Protocol", "ZoneAuthResponse: unknown status '" + tokens[0] + "'");
        return false;
    }
}

// ============================================================================
// MovementIntent (client ? ZoneServer)
// ============================================================================

std::string buildMovementIntentPayload(
    const MovementIntentData& data) {
    std::ostringstream oss;
    oss << data.characterId << '|'
        << data.sequenceNumber << '|'
        << data.inputX << '|'
        << data.inputY << '|'
        << data.facingYawDegrees << '|'
        << (data.isJumpPressed ? 1 : 0) << '|'
        << data.clientTimeMs;
    return oss.str();
}

bool parseMovementIntentPayload(
    const std::string& payload,
    MovementIntentData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 7) {
        req::shared::logError("Protocol", std::string{"MovementIntent: expected 7 fields, got "} + 
            std::to_string(tokens.size()) + ", payload='" + payload + "'");
        return false;
    }
    
    // Parse characterId
    if (!parseUInt(tokens[0], outData.characterId)) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse characterId from '"} + 
            tokens[0] + "', payload='" + payload + "'");
        return false;
    }
    
    // Parse sequenceNumber
    if (!parseUInt(tokens[1], outData.sequenceNumber)) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse sequenceNumber from '"} + 
            tokens[1] + "', payload='" + payload + "'");
        return false;
    }
    
    // Parse inputX
    try {
        outData.inputX = std::stof(tokens[2]);
    } catch (const std::exception& e) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse inputX from '"} + 
            tokens[2] + "': " + e.what() + ", payload='" + payload + "'");
        return false;
    } catch (...) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse inputX from '"} + 
            tokens[2] + "' (unknown exception), payload='" + payload + "'");
        return false;
    }
    
    // Parse inputY
    try {
        outData.inputY = std::stof(tokens[3]);
    } catch (const std::exception& e) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse inputY from '"} + 
            tokens[3] + "': " + e.what() + ", payload='" + payload + "'");
        return false;
    } catch (...) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse inputY from '"} + 
            tokens[3] + "' (unknown exception), payload='" + payload + "'");
        return false;
    }
    
    // Parse facingYawDegrees
    try {
        outData.facingYawDegrees = std::stof(tokens[4]);
    } catch (const std::exception& e) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse facingYawDegrees from '"} + 
            tokens[4] + "': " + e.what() + ", payload='" + payload + "'");
        return false;
    } catch (...) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse facingYawDegrees from '"} + 
            tokens[4] + "' (unknown exception), payload='" + payload + "'");
        return false;
    }
    
    // Parse isJumpPressed (0 or 1)
    std::uint32_t jumpValue = 0;
    if (!parseUInt(tokens[5], jumpValue)) {
        req::shared::logError("Protocol", std::string{"MovementIntent: failed to parse isJumpPressed from '"} + 
            tokens[5] + "', payload='" + payload + "'");
        return false;
    }
    outData.isJumpPressed = (jumpValue != 0);
    
    // Parse clientTimeMs (tolerant - default to 0 on failure, don't fail entire parse)
    try {
        outData.clientTimeMs = std::stoull(tokens[6]);
    } catch (const std::exception& e) {
        req::shared::logWarn("Protocol", std::string{"MovementIntent: invalid clientTimeMs '"} + 
            tokens[6] + "' (" + e.what() + "), defaulting to 0");
        outData.clientTimeMs = 0;
    } catch (...) {
        req::shared::logWarn("Protocol", std::string{"MovementIntent: invalid clientTimeMs '"} + 
            tokens[6] + "' (unknown exception), defaulting to 0");
        outData.clientTimeMs = 0;
    }
    
    return true;
}

// ============================================================================
// PlayerStateSnapshot (ZoneServer ? client)
// ============================================================================

std::string buildPlayerStateSnapshotPayload(
    const PlayerStateSnapshotData& data) {
    std::ostringstream oss;
    oss << data.snapshotId << '|' << data.players.size();
    
    for (const auto& player : data.players) {
        oss << '|' << player.characterId << ','
            << player.posX << ','
            << player.posY << ','
            << player.posZ << ','
            << player.velX << ','
            << player.velY << ','
            << player.velZ << ','
            << player.yawDegrees;
    }
    
    return oss.str();
}

bool parsePlayerStateSnapshotPayload(
    const std::string& payload,
    PlayerStateSnapshotData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        req::shared::logError("Protocol", "PlayerStateSnapshot: expected at least 2 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    
    // Parse snapshotId
    if (!parseUInt(tokens[0], outData.snapshotId)) {
        req::shared::logError("Protocol", "PlayerStateSnapshot: failed to parse snapshotId");
        return false;
    }
    
    // Parse playerCount
    std::size_t playerCount = 0;
    if (!parseUInt(tokens[1], playerCount)) {
        req::shared::logError("Protocol", "PlayerStateSnapshot: failed to parse playerCount");
        return false;
    }
    
    // Check if we have the expected number of player entries
    std::size_t actualPlayerCount = tokens.size() - 2;
    if (actualPlayerCount != playerCount) {
        req::shared::logWarn("Protocol", std::string{"PlayerStateSnapshot: playerCount mismatch - expected "} +
            std::to_string(playerCount) + ", got " + std::to_string(actualPlayerCount) + " entries");
        // Continue parsing with actual count (tolerant approach)
    }
    
    // Parse each player entry
    outData.players.clear();
    for (std::size_t i = 2; i < tokens.size(); ++i) {
        auto playerTokens = split(tokens[i], ',');
        if (playerTokens.size() < 8) {
            req::shared::logError("Protocol", std::string{"PlayerStateSnapshot: player entry "} + 
                std::to_string(i - 2) + " malformed (expected 8 fields, got " + 
                std::to_string(playerTokens.size()) + ")");
            return false;
        }
        
        PlayerStateEntry entry;
        
        // Parse characterId
        if (!parseUInt(playerTokens[0], entry.characterId)) {
            req::shared::logError("Protocol", "PlayerStateSnapshot: failed to parse player characterId");
            return false;
        }
        
        // Parse position (posX, posY, posZ)
        try {
            entry.posX = std::stof(playerTokens[1]);
            entry.posY = std::stof(playerTokens[2]);
            entry.posZ = std::stof(playerTokens[3]);
        } catch (...) {
            req::shared::logError("Protocol", "PlayerStateSnapshot: failed to parse player position");
            return false;
        }
        
        // Parse velocity (velX, velY, velZ)
        try {
            entry.velX = std::stof(playerTokens[4]);
            entry.velY = std::stof(playerTokens[5]);
            entry.velZ = std::stof(playerTokens[6]);
        } catch (...) {
            req::shared::logError("Protocol", "PlayerStateSnapshot: failed to parse player velocity");
            return false;
        }
        
        // Parse yawDegrees
        try {
            entry.yawDegrees = std::stof(playerTokens[7]);
        } catch (...) {
            req::shared::logError("Protocol", "PlayerStateSnapshot: failed to parse player yawDegrees");
            return false;
        }
        
        outData.players.push_back(entry);
    }
    
    return true;
}

} // namespace req::shared::protocol
