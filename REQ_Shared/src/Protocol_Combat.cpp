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
