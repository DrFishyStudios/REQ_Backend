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
// LoginRequest
// ============================================================================

std::string buildLoginRequestPayload(
    const std::string& username,
    const std::string& password,
    const std::string& clientVersion,
    LoginMode mode) {
    std::ostringstream oss;
    oss << username << '|' << password << '|' << clientVersion << '|';
    oss << (mode == LoginMode::Register ? "register" : "login");
    return oss.str();
}

bool parseLoginRequestPayload(
    const std::string& payload,
    std::string& outUsername,
    std::string& outPassword,
    std::string& outClientVersion,
    LoginMode& outMode) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 3) {
        req::shared::logError("Protocol", "LoginRequest: expected at least 3 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    outUsername = tokens[0];
    outPassword = tokens[1];
    outClientVersion = tokens[2];
    
    // Mode field is optional for backward compatibility - defaults to "login"
    if (tokens.size() >= 4) {
        if (tokens[3] == "register") {
            outMode = LoginMode::Register;
        } else if (tokens[3] == "login") {
            outMode = LoginMode::Login;
        } else {
            req::shared::logWarn("Protocol", "LoginRequest: unknown mode '" + tokens[3] + "', defaulting to login");
            outMode = LoginMode::Login;
        }
    } else {
        outMode = LoginMode::Login;
    }
    
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

} // namespace req::shared::protocol
