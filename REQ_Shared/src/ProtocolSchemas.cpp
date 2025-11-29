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
        req::shared::logError("Protocol", "MovementIntent: expected 7 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    
    // Parse characterId
    if (!parseUInt(tokens[0], outData.characterId)) {
        req::shared::logError("Protocol", "MovementIntent: failed to parse characterId");
        return false;
    }
    
    // Parse sequenceNumber
    if (!parseUInt(tokens[1], outData.sequenceNumber)) {
        req::shared::logError("Protocol", "MovementIntent: failed to parse sequenceNumber");
        return false;
    }
    
    // Parse inputX
    try {
        outData.inputX = std::stof(tokens[2]);
    } catch (...) {
        req::shared::logError("Protocol", "MovementIntent: failed to parse inputX");
        return false;
    }
    
    // Parse inputY
    try {
        outData.inputY = std::stof(tokens[3]);
    } catch (...) {
        req::shared::logError("Protocol", "MovementIntent: failed to parse inputY");
        return false;
    }
    
    // Parse facingYawDegrees
    try {
        outData.facingYawDegrees = std::stof(tokens[4]);
    } catch (...) {
        req::shared::logError("Protocol", "MovementIntent: failed to parse facingYawDegrees");
        return false;
    }
    
    // Parse isJumpPressed (0 or 1)
    std::uint32_t jumpValue = 0;
    if (!parseUInt(tokens[5], jumpValue)) {
        req::shared::logError("Protocol", "MovementIntent: failed to parse isJumpPressed");
        return false;
    }
    outData.isJumpPressed = (jumpValue != 0);
    
    // Parse clientTimeMs
    if (!parseUInt(tokens[6], outData.clientTimeMs)) {
        req::shared::logError("Protocol", "MovementIntent: failed to parse clientTimeMs");
        return false;
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

// ============================================================================
// AttackRequest (client ? ZoneServer)
// ============================================================================

std::string buildAttackRequestPayload(
    const AttackRequestData& data) {
    std::ostringstream oss;
    oss << data.attackerCharacterId << '|'
        << data.targetId << '|'
        << data.abilityId << '|'
        << (data.isBasicAttack ? 1 : 0);
    return oss.str();
}

bool parseAttackRequestPayload(
    const std::string& payload,
    AttackRequestData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 4) {
        req::shared::logError("Protocol", "AttackRequest: expected 4 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    
    // Parse attackerCharacterId
    if (!parseUInt(tokens[0], outData.attackerCharacterId)) {
        req::shared::logError("Protocol", "AttackRequest: failed to parse attackerCharacterId");
        return false;
    }
    
    // Parse targetId
    if (!parseUInt(tokens[1], outData.targetId)) {
        req::shared::logError("Protocol", "AttackRequest: failed to parse targetId");
        return false;
    }
    
    // Parse abilityId
    if (!parseUInt(tokens[2], outData.abilityId)) {
        req::shared::logError("Protocol", "AttackRequest: failed to parse abilityId");
        return false;
    }
    
    // Parse isBasicAttack (0 or 1)
    std::uint32_t basicAttackValue = 0;
    if (!parseUInt(tokens[3], basicAttackValue)) {
        req::shared::logError("Protocol", "AttackRequest: failed to parse isBasicAttack");
        return false;
    }
    outData.isBasicAttack = (basicAttackValue != 0);
    
    return true;
}

// ============================================================================
// AttackResult (ZoneServer ? client)
// ============================================================================

std::string buildAttackResultPayload(
    const AttackResultData& data) {
    std::ostringstream oss;
    oss << data.attackerId << '|'
        << data.targetId << '|'
        << data.damage << '|'
        << (data.wasHit ? 1 : 0) << '|'
        << data.remainingHp << '|'
        << data.resultCode << '|'
        << data.message;
    return oss.str();
}

bool parseAttackResultPayload(
    const std::string& payload,
    AttackResultData& outData) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 7) {
        req::shared::logError("Protocol", "AttackResult: expected 7 fields, got " + std::to_string(tokens.size()));
        return false;
    }
    
    // Parse attackerId
    if (!parseUInt(tokens[0], outData.attackerId)) {
        req::shared::logError("Protocol", "AttackResult: failed to parse attackerId");
        return false;
    }
    
    // Parse targetId
    if (!parseUInt(tokens[1], outData.targetId)) {
        req::shared::logError("Protocol", "AttackResult: failed to parse targetId");
        return false;
    }
    
    // Parse damage
    try {
        outData.damage = std::stoi(tokens[2]);
    } catch (...) {
        req::shared::logError("Protocol", "AttackResult: failed to parse damage");
        return false;
    }
    
    // Parse wasHit (0 or 1)
    std::uint32_t hitValue = 0;
    if (!parseUInt(tokens[3], hitValue)) {
        req::shared::logError("Protocol", "AttackResult: failed to parse wasHit");
        return false;
    }
    outData.wasHit = (hitValue != 0);
    
    // Parse remainingHp
    try {
        outData.remainingHp = std::stoi(tokens[4]);
    } catch (...) {
        req::shared::logError("Protocol", "AttackResult: failed to parse remainingHp");
        return false;
    }
    
    // Parse resultCode
    try {
        outData.resultCode = std::stoi(tokens[5]);
    } catch (...) {
        req::shared::logError("Protocol", "AttackResult: failed to parse resultCode");
        return false;
    }
    
    // Parse message (rest of payload, may contain spaces but not pipes)
    outData.message = tokens[6];
    
    return true;
}

} // namespace req::shared::protocol
