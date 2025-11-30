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
// CharacterListRequest
// ============================================================================

std::string buildCharacterListRequestPayload(
    SessionToken sessionToken,
    WorldId worldId) {
    std::ostringstream oss;
    oss << sessionToken << '|' << worldId;
    return oss.str();
}

bool parseCharacterListRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        req::shared::logError("Protocol", "CharacterListRequest: expected 2 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    if (!parseUInt(tokens[0], outSessionToken)) {
        req::shared::logError("Protocol", "CharacterListRequest: failed to parse sessionToken");
        return false;
    }
    if (!parseUInt(tokens[1], outWorldId)) {
        req::shared::logError("Protocol", "CharacterListRequest: failed to parse worldId");
        return false;
    }
    return true;
}

// ============================================================================
// CharacterListResponse
// ============================================================================

std::string buildCharacterListResponseOkPayload(
    const std::vector<CharacterListEntry>& characters) {
    std::ostringstream oss;
    oss << "OK|" << characters.size();
    for (const auto& ch : characters) {
        oss << '|' << ch.characterId << ',' << ch.name << ',' << ch.race << ',' 
            << ch.characterClass << ',' << ch.level;
    }
    return oss.str();
}

std::string buildCharacterListResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage) {
    std::ostringstream oss;
    oss << "ERR|" << errorCode << '|' << errorMessage;
    return oss.str();
}

bool parseCharacterListResponsePayload(
    const std::string& payload,
    CharacterListResponseData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.empty()) {
        req::shared::logError("Protocol", "CharacterListResponse: empty payload");
        return false;
    }

    if (tokens[0] == "OK") {
        if (tokens.size() < 2) {
            req::shared::logError("Protocol", "CharacterListResponse OK: expected at least 2 fields");
            return false;
        }
        outData.success = true;
        std::size_t charCount = 0;
        if (!parseUInt(tokens[1], charCount)) {
            req::shared::logError("Protocol", "CharacterListResponse: failed to parse character count");
            return false;
        }
        if (tokens.size() != 2 + charCount) {
            req::shared::logError("Protocol", "CharacterListResponse: character count mismatch");
            return false;
        }
        outData.characters.clear();
        for (std::size_t i = 0; i < charCount; ++i) {
            auto charTokens = split(tokens[2 + i], ',');
            if (charTokens.size() < 5) {
                req::shared::logError("Protocol", "CharacterListResponse: character entry malformed");
                return false;
            }
            CharacterListEntry entry;
            if (!parseUInt(charTokens[0], entry.characterId)) return false;
            entry.name = charTokens[1];
            entry.race = charTokens[2];
            entry.characterClass = charTokens[3];
            if (!parseUInt(charTokens[4], entry.level)) return false;
            outData.characters.push_back(entry);
        }
        return true;
    } else if (tokens[0] == "ERR") {
        if (tokens.size() < 3) {
            req::shared::logError("Protocol", "CharacterListResponse ERR: expected 3 fields");
            return false;
        }
        outData.success = false;
        outData.errorCode = tokens[1];
        outData.errorMessage = tokens[2];
        return true;
    } else {
        req::shared::logError("Protocol", "CharacterListResponse: unknown status '" + tokens[0] + "'");
        return false;
    }
}

// ============================================================================
// CharacterCreateRequest
// ============================================================================

std::string buildCharacterCreateRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass) {
    std::ostringstream oss;
    oss << sessionToken << '|' << worldId << '|' << name << '|' << race << '|' << characterClass;
    return oss.str();
}

bool parseCharacterCreateRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId,
    std::string& outName,
    std::string& outRace,
    std::string& outClass) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 5) {
        req::shared::logError("Protocol", "CharacterCreateRequest: expected 5 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    if (!parseUInt(tokens[0], outSessionToken)) {
        req::shared::logError("Protocol", "CharacterCreateRequest: failed to parse sessionToken");
        return false;
    }
    if (!parseUInt(tokens[1], outWorldId)) {
        req::shared::logError("Protocol", "CharacterCreateRequest: failed to parse worldId");
        return false;
    }
    outName = tokens[2];
    outRace = tokens[3];
    outClass = tokens[4];
    return true;
}

// ============================================================================
// CharacterCreateResponse
// ============================================================================

std::string buildCharacterCreateResponseOkPayload(
    std::uint64_t characterId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass,
    std::uint32_t level) {
    std::ostringstream oss;
    oss << "OK|" << characterId << '|' << name << '|' << race << '|' << characterClass << '|' << level;
    return oss.str();
}

std::string buildCharacterCreateResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage) {
    std::ostringstream oss;
    oss << "ERR|" << errorCode << '|' << errorMessage;
    return oss.str();
}

bool parseCharacterCreateResponsePayload(
    const std::string& payload,
    CharacterCreateResponseData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.empty()) {
        req::shared::logError("Protocol", "CharacterCreateResponse: empty payload");
        return false;
    }

    if (tokens[0] == "OK") {
        if (tokens.size() < 6) {
            req::shared::logError("Protocol", "CharacterCreateResponse OK: expected 6 fields");
            return false;
        }
        outData.success = true;
        if (!parseUInt(tokens[1], outData.characterId)) {
            req::shared::logError("Protocol", "CharacterCreateResponse: failed to parse characterId");
            return false;
        }
        outData.name = tokens[2];
        outData.race = tokens[3];
        outData.characterClass = tokens[4];
        if (!parseUInt(tokens[5], outData.level)) {
            req::shared::logError("Protocol", "CharacterCreateResponse: failed to parse level");
            return false;
        }
        return true;
    } else if (tokens[0] == "ERR") {
        if (tokens.size() < 3) {
            req::shared::logError("Protocol", "CharacterCreateResponse ERR: expected 3 fields");
            return false;
        }
        outData.success = false;
        outData.errorCode = tokens[1];
        outData.errorMessage = tokens[2];
        return true;
    } else {
        req::shared::logError("Protocol", "CharacterCreateResponse: unknown status '" + tokens[0] + "'");
        return false;
    }
}

// ============================================================================
// EnterWorldRequest
// ============================================================================

std::string buildEnterWorldRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    std::uint64_t characterId) {
    std::ostringstream oss;
    oss << sessionToken << '|' << worldId << '|' << characterId;
    return oss.str();
}

bool parseEnterWorldRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId,
    std::uint64_t& outCharacterId) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 3) {
        req::shared::logError("Protocol", "EnterWorldRequest: expected 3 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    if (!parseUInt(tokens[0], outSessionToken)) {
        req::shared::logError("Protocol", "EnterWorldRequest: failed to parse sessionToken");
        return false;
    }
    if (!parseUInt(tokens[1], outWorldId)) {
        req::shared::logError("Protocol", "EnterWorldRequest: failed to parse worldId");
        return false;
    }
    if (!parseUInt(tokens[2], outCharacterId)) {
        req::shared::logError("Protocol", "EnterWorldRequest: failed to parse characterId");
        return false;
    }
    return true;
}

// ============================================================================
// EnterWorldResponse
// ============================================================================

std::string buildEnterWorldResponseOkPayload(
    HandoffToken handoffToken,
    ZoneId zoneId,
    const std::string& zoneHost,
    std::uint16_t zonePort) {
    std::ostringstream oss;
    oss << "OK|" << handoffToken << '|' << zoneId << '|' << zoneHost << '|' << zonePort;
    return oss.str();
}

std::string buildEnterWorldResponseErrorPayload(
    const std::string& errorCode,
    const std::string& errorMessage) {
    std::ostringstream oss;
    oss << "ERR|" << errorCode << '|' << errorMessage;
    return oss.str();
}

bool parseEnterWorldResponsePayload(
    const std::string& payload,
    EnterWorldResponseData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.empty()) {
        req::shared::logError("Protocol", "EnterWorldResponse: empty payload");
        return false;
    }

    if (tokens[0] == "OK") {
        if (tokens.size() < 5) {
            req::shared::logError("Protocol", "EnterWorldResponse OK: expected 5 fields");
            return false;
        }
        outData.success = true;
        if (!parseUInt(tokens[1], outData.handoffToken)) {
            req::shared::logError("Protocol", "EnterWorldResponse: failed to parse handoffToken");
            return false;
        }
        if (!parseUInt(tokens[2], outData.zoneId)) {
            req::shared::logError("Protocol", "EnterWorldResponse: failed to parse zoneId");
            return false;
        }
        outData.zoneHost = tokens[3];
        if (!parseUInt(tokens[4], outData.zonePort)) {
            req::shared::logError("Protocol", "EnterWorldResponse: failed to parse zonePort");
            return false;
        }
        return true;
    } else if (tokens[0] == "ERR") {
        if (tokens.size() < 3) {
            req::shared::logError("Protocol", "EnterWorldResponse ERR: expected 3 fields");
            return false;
        }
        outData.success = false;
        outData.errorCode = tokens[1];
        outData.errorMessage = tokens[2];
        return true;
    } else {
        req::shared::logError("Protocol", "EnterWorldResponse: unknown status '" + tokens[0] + "'");
        return false;
    }
}

} // namespace req::shared::protocol
