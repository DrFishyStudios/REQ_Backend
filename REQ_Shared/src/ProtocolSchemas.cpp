#include "../include/req/shared/ProtocolSchemas.h"
#include "../include/req/shared/Logger.h"

#include <sstream>
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
// LoginRequest
// ============================================================================

std::string buildLoginRequestPayload(
    const std::string& username,
    const std::string& password,
    const std::string& clientVersion) {
    std::ostringstream oss;
    oss << username << '|' << password << '|' << clientVersion;
    return oss.str();
}

bool parseLoginRequestPayload(
    const std::string& payload,
    std::string& outUsername,
    std::string& outPassword,
    std::string& outClientVersion) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 3) {
        req::shared::logError("Protocol", "LoginRequest: expected 3 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    outUsername = tokens[0];
    outPassword = tokens[1];
    outClientVersion = tokens[2];
    return true;
}

// ============================================================================
// LoginResponse
// ============================================================================

std::string buildLoginResponseOkPayload(
    SessionToken token,
    const std::vector<WorldListEntry>& worlds) {
    std::ostringstream oss;
    oss << "OK|" << token << '|' << worlds.size();
    for (const auto& w : worlds) {
        oss << '|' << w.worldId << ',' << w.worldName << ',' << w.worldHost << ',' << w.worldPort << ',' << w.rulesetId;
    }
    return oss.str();
}

std::string buildLoginResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage) {
    std::ostringstream oss;
    oss << "ERR|" << errorCode << '|' << errorMessage;
    return oss.str();
}

bool parseLoginResponsePayload(
    const std::string& payload,
    LoginResponseData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.empty()) {
        req::shared::logError("Protocol", "LoginResponse: empty payload");
        return false;
    }

    if (tokens[0] == "OK") {
        if (tokens.size() < 3) {
            req::shared::logError("Protocol", "LoginResponse OK: expected at least 3 fields");
            return false;
        }
        outData.success = true;
        if (!parseUInt(tokens[1], outData.sessionToken)) {
            req::shared::logError("Protocol", "LoginResponse: failed to parse sessionToken");
            return false;
        }
        std::size_t worldCount = 0;
        if (!parseUInt(tokens[2], worldCount)) {
            req::shared::logError("Protocol", "LoginResponse: failed to parse worldCount");
            return false;
        }
        if (tokens.size() != 3 + worldCount) {
            req::shared::logError("Protocol", "LoginResponse: world count mismatch");
            return false;
        }
        outData.worlds.clear();
        for (std::size_t i = 0; i < worldCount; ++i) {
            auto worldTokens = split(tokens[3 + i], ',');
            if (worldTokens.size() < 5) {
                req::shared::logError("Protocol", "LoginResponse: world entry malformed");
                return false;
            }
            WorldListEntry entry;
            if (!parseUInt(worldTokens[0], entry.worldId)) return false;
            entry.worldName = worldTokens[1];
            entry.worldHost = worldTokens[2];
            if (!parseUInt(worldTokens[3], entry.worldPort)) return false;
            entry.rulesetId = worldTokens[4];
            outData.worlds.push_back(entry);
        }
        return true;
    } else if (tokens[0] == "ERR") {
        if (tokens.size() < 3) {
            req::shared::logError("Protocol", "LoginResponse ERR: expected 3 fields");
            return false;
        }
        outData.success = false;
        outData.errorCode = tokens[1];
        outData.errorMessage = tokens[2];
        return true;
    } else {
        req::shared::logError("Protocol", "LoginResponse: unknown status '" + tokens[0] + "'");
        return false;
    }
}

// ============================================================================
// WorldAuthRequest
// ============================================================================

std::string buildWorldAuthRequestPayload(
    SessionToken sessionToken,
    WorldId worldId) {
    std::ostringstream oss;
    oss << sessionToken << '|' << worldId;
    return oss.str();
}

bool parseWorldAuthRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        req::shared::logError("Protocol", "WorldAuthRequest: expected 2 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    if (!parseUInt(tokens[0], outSessionToken)) {
        req::shared::logError("Protocol", "WorldAuthRequest: failed to parse sessionToken");
        return false;
    }
    if (!parseUInt(tokens[1], outWorldId)) {
        req::shared::logError("Protocol", "WorldAuthRequest: failed to parse worldId");
        return false;
    }
    return true;
}

// ============================================================================
// WorldAuthResponse
// ============================================================================

std::string buildWorldAuthResponseOkPayload(
    HandoffToken handoffToken,
    ZoneId zoneId,
    const std::string& zoneHost,
    std::uint16_t zonePort) {
    std::ostringstream oss;
    oss << "OK|" << handoffToken << '|' << zoneId << '|' << zoneHost << '|' << zonePort;
    return oss.str();
}

std::string buildWorldAuthResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage) {
    std::ostringstream oss;
    oss << "ERR|" << errorCode << '|' << errorMessage;
    return oss.str();
}

bool parseWorldAuthResponsePayload(
    const std::string& payload,
    WorldAuthResponseData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.empty()) {
        req::shared::logError("Protocol", "WorldAuthResponse: empty payload");
        return false;
    }

    if (tokens[0] == "OK") {
        if (tokens.size() < 5) {
            req::shared::logError("Protocol", "WorldAuthResponse OK: expected 5 fields");
            return false;
        }
        outData.success = true;
        if (!parseUInt(tokens[1], outData.handoffToken)) {
            req::shared::logError("Protocol", "WorldAuthResponse: failed to parse handoffToken");
            return false;
        }
        if (!parseUInt(tokens[2], outData.zoneId)) {
            req::shared::logError("Protocol", "WorldAuthResponse: failed to parse zoneId");
            return false;
        }
        outData.zoneHost = tokens[3];
        if (!parseUInt(tokens[4], outData.zonePort)) {
            req::shared::logError("Protocol", "WorldAuthResponse: failed to parse zonePort");
            return false;
        }
        return true;
    } else if (tokens[0] == "ERR") {
        if (tokens.size() < 3) {
            req::shared::logError("Protocol", "WorldAuthResponse ERR: expected 3 fields");
            return false;
        }
        outData.success = false;
        outData.errorCode = tokens[1];
        outData.errorMessage = tokens[2];
        return true;
    } else {
        req::shared::logError("Protocol", "WorldAuthResponse: unknown status '" + tokens[0] + "'");
        return false;
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

} // namespace req::shared::protocol
