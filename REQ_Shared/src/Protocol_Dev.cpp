#include "../include/req/shared/ProtocolSchemas.h"
#include "../include/req/shared/Logger.h"

#include <sstream>
#include <string>
#include <vector>

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
        if (s.empty()) return false;
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
// DevCommand (client ? ZoneServer)
// ============================================================================

std::string buildDevCommandPayload(
    const DevCommandData& data) {
    std::ostringstream oss;
    oss << data.characterId << '|'
        << data.command << '|'
        << data.param1 << '|'
        << data.param2;
    return oss.str();
}

bool parseDevCommandPayload(
    const std::string& payload,
    DevCommandData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 4) {
        req::shared::logError("Protocol", "DevCommand: expected 4 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    
    // Parse characterId
    if (!parseUInt(tokens[0], outData.characterId)) {
        req::shared::logError("Protocol", "DevCommand: failed to parse characterId");
        return false;
    }
    
    // Parse command
    outData.command = tokens[1];
    
    // Parse param1 (may be empty)
    outData.param1 = tokens[2];
    
    // Parse param2 (may be empty)
    outData.param2 = tokens[3];
    
    return true;
}

// ============================================================================
// DevCommandResponse (ZoneServer ? client)
// ============================================================================

std::string buildDevCommandResponsePayload(
    const DevCommandResponseData& data) {
    std::ostringstream oss;
    oss << (data.success ? 1 : 0) << '|'
        << data.message;
    return oss.str();
}

bool parseDevCommandResponsePayload(
    const std::string& payload,
    DevCommandResponseData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        req::shared::logError("Protocol", "DevCommandResponse: expected 2 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    
    // Parse success (0 or 1)
    std::uint32_t successValue = 0;
    if (!parseUInt(tokens[0], successValue)) {
        req::shared::logError("Protocol", "DevCommandResponse: failed to parse success");
        return false;
    }
    outData.success = (successValue != 0);
    
    // Parse message
    outData.message = tokens[1];
    
    return true;
}

} // namespace req::shared::protocol
