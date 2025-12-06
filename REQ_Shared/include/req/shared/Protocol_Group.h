#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace req::shared::protocol {

// ============================================================================
// Group System Protocol (Phase 3)
// ============================================================================

/**
 * GroupInviteRequestData
 * Client requests to invite another player to their group
 */
struct GroupInviteRequestData {
    std::uint64_t inviterCharacterId{ 0 };  // Character doing the inviting
    std::string targetName;                  // Name of character to invite
};

/**
 * GroupInviteResponseData
 * Server responds with result of invite request
 */
struct GroupInviteResponseData {
    bool success{ false };
    std::uint64_t groupId{ 0 };              // Group ID if successful
    std::string errorCode;                   // Error code if failed
    std::string errorMessage;                // Human-readable error
};

/**
 * GroupAcceptRequestData
 * Client accepts a group invite
 */
struct GroupAcceptRequestData {
    std::uint64_t characterId{ 0 };
    std::uint64_t groupId{ 0 };
};

/**
 * GroupDeclineRequestData
 * Client declines a group invite
 */
struct GroupDeclineRequestData {
    std::uint64_t characterId{ 0 };
    std::uint64_t groupId{ 0 };
};

/**
 * GroupLeaveRequestData
 * Client requests to leave their current group
 */
struct GroupLeaveRequestData {
    std::uint64_t characterId{ 0 };
};

/**
 * GroupKickRequestData
 * Client (leader) requests to kick a member from the group
 */
struct GroupKickRequestData {
    std::uint64_t leaderCharacterId{ 0 };
    std::uint64_t targetCharacterId{ 0 };
};

/**
 * GroupDisbandRequestData
 * Client (leader) requests to disband the group
 */
struct GroupDisbandRequestData {
    std::uint64_t leaderCharacterId{ 0 };
};

/**
 * GroupMemberInfo
 * Information about a group member
 */
struct GroupMemberInfo {
    std::uint64_t characterId{ 0 };
    std::string name;
    std::uint32_t level{ 1 };
    std::string characterClass;
    std::int32_t hp{ 100 };
    std::int32_t maxHp{ 100 };
    std::int32_t mana{ 100 };
    std::int32_t maxMana{ 100 };
    bool isLeader{ false };
};

/**
 * GroupUpdateNotifyData
 * Server notifies client of group membership changes
 */
struct GroupUpdateNotifyData {
    std::uint64_t groupId{ 0 };
    std::uint64_t leaderCharacterId{ 0 };
    std::vector<GroupMemberInfo> members;
    std::string updateType;  // "created", "joined", "left", "kicked", "disbanded"
};

/**
 * GroupChatMessageData
 * Group chat message
 */
struct GroupChatMessageData {
    std::uint64_t senderCharacterId{ 0 };
    std::string senderName;
    std::string message;
    std::uint64_t groupId{ 0 };
};

// ============================================================================
// Build Functions
// ============================================================================

std::string buildGroupInviteRequestPayload(const GroupInviteRequestData& data);
std::string buildGroupInviteResponsePayload(const GroupInviteResponseData& data);
std::string buildGroupAcceptRequestPayload(const GroupAcceptRequestData& data);
std::string buildGroupDeclineRequestPayload(const GroupDeclineRequestData& data);
std::string buildGroupLeaveRequestPayload(const GroupLeaveRequestData& data);
std::string buildGroupKickRequestPayload(const GroupKickRequestData& data);
std::string buildGroupDisbandRequestPayload(const GroupDisbandRequestData& data);
std::string buildGroupUpdateNotifyPayload(const GroupUpdateNotifyData& data);
std::string buildGroupChatMessagePayload(const GroupChatMessageData& data);

// ============================================================================
// Parse Functions
// ============================================================================

bool parseGroupInviteRequestPayload(const std::string& payload, GroupInviteRequestData& out);
bool parseGroupInviteResponsePayload(const std::string& payload, GroupInviteResponseData& out);
bool parseGroupAcceptRequestPayload(const std::string& payload, GroupAcceptRequestData& out);
bool parseGroupDeclineRequestPayload(const std::string& payload, GroupDeclineRequestData& out);
bool parseGroupLeaveRequestPayload(const std::string& payload, GroupLeaveRequestData& out);
bool parseGroupKickRequestPayload(const std::string& payload, GroupKickRequestData& out);
bool parseGroupDisbandRequestPayload(const std::string& payload, GroupDisbandRequestData& out);
bool parseGroupUpdateNotifyPayload(const std::string& payload, GroupUpdateNotifyData& out);
bool parseGroupChatMessagePayload(const std::string& payload, GroupChatMessageData& out);

} // namespace req::shared::protocol
