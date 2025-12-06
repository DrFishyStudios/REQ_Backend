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
// GroupInviteRequest
// Format: inviterCharacterId|targetName
// ============================================================================

std::string buildGroupInviteRequestPayload(const GroupInviteRequestData& data) {
    std::ostringstream oss;
    oss << data.inviterCharacterId << '|' << data.targetName;
    return oss.str();
}

bool parseGroupInviteRequestPayload(const std::string& payload, GroupInviteRequestData& out) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        logError("Protocol", "GroupInviteRequest: expected 2 fields");
        return false;
    }
    
    if (!parseUInt(tokens[0], out.inviterCharacterId)) {
        logError("Protocol", "GroupInviteRequest: failed to parse inviterCharacterId");
        return false;
    }
    
    out.targetName = tokens[1];
    return true;
}

// ============================================================================
// GroupInviteResponse
// Format: success|groupId|errorCode|errorMessage
// ============================================================================

std::string buildGroupInviteResponsePayload(const GroupInviteResponseData& data) {
    std::ostringstream oss;
    oss << (data.success ? 1 : 0) << '|'
        << data.groupId << '|'
        << data.errorCode << '|'
        << data.errorMessage;
    return oss.str();
}

bool parseGroupInviteResponsePayload(const std::string& payload, GroupInviteResponseData& out) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 4) {
        logError("Protocol", "GroupInviteResponse: expected 4 fields");
        return false;
    }
    
    std::uint32_t successValue = 0;
    if (!parseUInt(tokens[0], successValue)) {
        logError("Protocol", "GroupInviteResponse: failed to parse success");
        return false;
    }
    out.success = (successValue != 0);
    
    if (!parseUInt(tokens[1], out.groupId)) {
        logError("Protocol", "GroupInviteResponse: failed to parse groupId");
        return false;
    }
    
    out.errorCode = tokens[2];
    out.errorMessage = tokens[3];
    return true;
}

// ============================================================================
// GroupAcceptRequest
// Format: characterId|groupId
// ============================================================================

std::string buildGroupAcceptRequestPayload(const GroupAcceptRequestData& data) {
    std::ostringstream oss;
    oss << data.characterId << '|' << data.groupId;
    return oss.str();
}

bool parseGroupAcceptRequestPayload(const std::string& payload, GroupAcceptRequestData& out) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        logError("Protocol", "GroupAcceptRequest: expected 2 fields");
        return false;
    }
    
    if (!parseUInt(tokens[0], out.characterId)) {
        logError("Protocol", "GroupAcceptRequest: failed to parse characterId");
        return false;
    }
    
    if (!parseUInt(tokens[1], out.groupId)) {
        logError("Protocol", "GroupAcceptRequest: failed to parse groupId");
        return false;
    }
    
    return true;
}

// ============================================================================
// GroupDeclineRequest
// Format: characterId|groupId
// ============================================================================

std::string buildGroupDeclineRequestPayload(const GroupDeclineRequestData& data) {
    std::ostringstream oss;
    oss << data.characterId << '|' << data.groupId;
    return oss.str();
}

bool parseGroupDeclineRequestPayload(const std::string& payload, GroupDeclineRequestData& out) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        logError("Protocol", "GroupDeclineRequest: expected 2 fields");
        return false;
    }
    
    if (!parseUInt(tokens[0], out.characterId)) {
        logError("Protocol", "GroupDeclineRequest: failed to parse characterId");
        return false;
    }
    
    if (!parseUInt(tokens[1], out.groupId)) {
        logError("Protocol", "GroupDeclineRequest: failed to parse groupId");
        return false;
    }
    
    return true;
}

// ============================================================================
// GroupLeaveRequest
// Format: characterId
// ============================================================================

std::string buildGroupLeaveRequestPayload(const GroupLeaveRequestData& data) {
    return std::to_string(data.characterId);
}

bool parseGroupLeaveRequestPayload(const std::string& payload, GroupLeaveRequestData& out) {
    if (!parseUInt(payload, out.characterId)) {
        logError("Protocol", "GroupLeaveRequest: failed to parse characterId");
        return false;
    }
    return true;
}

// ============================================================================
// GroupKickRequest
// Format: leaderCharacterId|targetCharacterId
// ============================================================================

std::string buildGroupKickRequestPayload(const GroupKickRequestData& data) {
    std::ostringstream oss;
    oss << data.leaderCharacterId << '|' << data.targetCharacterId;
    return oss.str();
}

bool parseGroupKickRequestPayload(const std::string& payload, GroupKickRequestData& out) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 2) {
        logError("Protocol", "GroupKickRequest: expected 2 fields");
        return false;
    }
    
    if (!parseUInt(tokens[0], out.leaderCharacterId)) {
        logError("Protocol", "GroupKickRequest: failed to parse leaderCharacterId");
        return false;
    }
    
    if (!parseUInt(tokens[1], out.targetCharacterId)) {
        logError("Protocol", "GroupKickRequest: failed to parse targetCharacterId");
        return false;
    }
    
    return true;
}

// ============================================================================
// GroupDisbandRequest
// Format: leaderCharacterId
// ============================================================================

std::string buildGroupDisbandRequestPayload(const GroupDisbandRequestData& data) {
    return std::to_string(data.leaderCharacterId);
}

bool parseGroupDisbandRequestPayload(const std::string& payload, GroupDisbandRequestData& out) {
    if (!parseUInt(payload, out.leaderCharacterId)) {
        logError("Protocol", "GroupDisbandRequest: failed to parse leaderCharacterId");
        return false;
    }
    return true;
}

// ============================================================================
// GroupUpdateNotify
// Format: groupId|leaderCharacterId|updateType|memberCount|member1_id|member1_name|member1_level|member1_class|member1_hp|member1_maxHp|member1_mana|member1_maxMana|member1_isLeader|...
// ============================================================================

std::string buildGroupUpdateNotifyPayload(const GroupUpdateNotifyData& data) {
    std::ostringstream oss;
    oss << data.groupId << '|'
        << data.leaderCharacterId << '|'
        << data.updateType << '|'
        << data.members.size();
    
    for (const auto& member : data.members) {
        oss << '|' << member.characterId
            << '|' << member.name
            << '|' << member.level
            << '|' << member.characterClass
            << '|' << member.hp
            << '|' << member.maxHp
            << '|' << member.mana
            << '|' << member.maxMana
            << '|' << (member.isLeader ? 1 : 0);
    }
    
    return oss.str();
}

bool parseGroupUpdateNotifyPayload(const std::string& payload, GroupUpdateNotifyData& out) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 4) {
        logError("Protocol", "GroupUpdateNotify: expected at least 4 fields");
        return false;
    }
    
    std::size_t idx = 0;
    
    if (!parseUInt(tokens[idx++], out.groupId)) {
        logError("Protocol", "GroupUpdateNotify: failed to parse groupId");
        return false;
    }
    
    if (!parseUInt(tokens[idx++], out.leaderCharacterId)) {
        logError("Protocol", "GroupUpdateNotify: failed to parse leaderCharacterId");
        return false;
    }
    
    out.updateType = tokens[idx++];
    
    std::uint32_t memberCount = 0;
    if (!parseUInt(tokens[idx++], memberCount)) {
        logError("Protocol", "GroupUpdateNotify: failed to parse memberCount");
        return false;
    }
    
    // Each member needs 9 fields
    const std::size_t expectedTokens = 4 + (memberCount * 9);
    if (tokens.size() < expectedTokens) {
        logError("Protocol", "GroupUpdateNotify: insufficient tokens for members");
        return false;
    }
    
    out.members.clear();
    for (std::uint32_t i = 0; i < memberCount; ++i) {
        GroupMemberInfo member;
        
        if (!parseUInt(tokens[idx++], member.characterId)) {
            logError("Protocol", "GroupUpdateNotify: failed to parse member characterId");
            return false;
        }
        
        member.name = tokens[idx++];
        
        if (!parseUInt(tokens[idx++], member.level)) {
            logError("Protocol", "GroupUpdateNotify: failed to parse member level");
            return false;
        }
        
        member.characterClass = tokens[idx++];
        
        try {
            member.hp = std::stoi(tokens[idx++]);
            member.maxHp = std::stoi(tokens[idx++]);
            member.mana = std::stoi(tokens[idx++]);
            member.maxMana = std::stoi(tokens[idx++]);
        } catch (...) {
            logError("Protocol", "GroupUpdateNotify: failed to parse member stats");
            return false;
        }
        
        std::uint32_t isLeaderValue = 0;
        if (!parseUInt(tokens[idx++], isLeaderValue)) {
            logError("Protocol", "GroupUpdateNotify: failed to parse member isLeader");
            return false;
        }
        member.isLeader = (isLeaderValue != 0);
        
        out.members.push_back(member);
    }
    
    return true;
}

// ============================================================================
// GroupChatMessage
// Format: senderCharacterId|senderName|groupId|message
// ============================================================================

std::string buildGroupChatMessagePayload(const GroupChatMessageData& data) {
    std::ostringstream oss;
    oss << data.senderCharacterId << '|'
        << data.senderName << '|'
        << data.groupId << '|'
        << data.message;
    return oss.str();
}

bool parseGroupChatMessagePayload(const std::string& payload, GroupChatMessageData& out) {
    auto tokens = split(payload, '|');
    if (tokens.size() < 4) {
        logError("Protocol", "GroupChatMessage: expected 4 fields");
        return false;
    }
    
    if (!parseUInt(tokens[0], out.senderCharacterId)) {
        logError("Protocol", "GroupChatMessage: failed to parse senderCharacterId");
        return false;
    }
    
    out.senderName = tokens[1];
    
    if (!parseUInt(tokens[2], out.groupId)) {
        logError("Protocol", "GroupChatMessage: failed to parse groupId");
        return false;
    }
    
    out.message = tokens[3];
    return true;
}

} // namespace req::shared::protocol
