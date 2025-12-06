#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"

#include <algorithm>
#include <chrono>

namespace req::zone {

namespace {
    constexpr std::size_t kMaxGroupSize = 6;
}

// ============================================================================
// Helper Functions
// ============================================================================

req::shared::data::Group* ZoneServer::getGroupById(std::uint64_t groupId) {
    auto it = groups_.find(groupId);
    if (it != groups_.end()) {
        return &it->second;
    }
    return nullptr;
}

req::shared::data::Group* ZoneServer::getGroupForCharacter(std::uint64_t characterId) {
    auto it = characterToGroupId_.find(characterId);
    if (it != characterToGroupId_.end()) {
        return getGroupById(it->second);
    }
    return nullptr;
}

bool ZoneServer::isCharacterInGroup(std::uint64_t characterId) const {
    return characterToGroupId_.find(characterId) != characterToGroupId_.end();
}

bool ZoneServer::isGroupFull(const req::shared::data::Group& group) const {
    return group.memberCharacterIds.size() >= kMaxGroupSize;
}

std::optional<std::uint64_t> ZoneServer::getCharacterGroup(std::uint64_t characterId) const {
    auto it = characterToGroupId_.find(characterId);
    if (it != characterToGroupId_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// Core Group Management
// ============================================================================

req::shared::data::Group& ZoneServer::createGroup(std::uint64_t leaderCharacterId) {
    std::uint64_t groupId = nextGroupId_++;
    
    req::shared::data::Group group;
    group.groupId = groupId;
    group.leaderCharacterId = leaderCharacterId;
    group.memberCharacterIds.push_back(leaderCharacterId);
    
    auto now = std::chrono::system_clock::now();
    group.createdAtUnix = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    groups_[groupId] = group;
    characterToGroupId_[leaderCharacterId] = groupId;
    
    req::shared::logInfo("zone", std::string{"[GROUP] Created groupId="} + 
        std::to_string(groupId) + ", leader=" + std::to_string(leaderCharacterId));
    
    return groups_[groupId];
}

bool ZoneServer::addMemberToGroup(std::uint64_t groupId, std::uint64_t characterId) {
    auto* group = getGroupById(groupId);
    if (!group) {
        req::shared::logWarn("zone", std::string{"[GROUP] Add member failed: group not found, groupId="} + 
            std::to_string(groupId));
        return false;
    }
    
    if (isGroupFull(*group)) {
        req::shared::logWarn("zone", std::string{"[GROUP] Add member failed: group full, groupId="} + 
            std::to_string(groupId));
        return false;
    }
    
    if (isCharacterInGroup(characterId)) {
        req::shared::logWarn("zone", std::string{"[GROUP] Add member failed: already in group, characterId="} + 
            std::to_string(characterId));
        return false;
    }
    
    group->memberCharacterIds.push_back(characterId);
    characterToGroupId_[characterId] = groupId;
    
    req::shared::logInfo("zone", std::string{"[GROUP] Added member="} + 
        std::to_string(characterId) + " to groupId=" + std::to_string(groupId));
    
    return true;
}

bool ZoneServer::removeMemberFromGroup(std::uint64_t groupId, std::uint64_t characterId) {
    auto* group = getGroupById(groupId);
    if (!group) {
        req::shared::logWarn("zone", std::string{"[GROUP] Remove member failed: group not found, groupId="} + 
            std::to_string(groupId));
        return false;
    }
    
    // Remove from member list
    auto& members = group->memberCharacterIds;
    auto it = std::find(members.begin(), members.end(), characterId);
    if (it == members.end()) {
        req::shared::logWarn("zone", std::string{"[GROUP] Remove member failed: not in group, characterId="} + 
            std::to_string(characterId));
        return false;
    }
    
    members.erase(it);
    characterToGroupId_.erase(characterId);
    
    req::shared::logInfo("zone", std::string{"[GROUP] Removed member="} + 
        std::to_string(characterId) + " from groupId=" + std::to_string(groupId));
    
    // If group is now empty, disband it
    if (members.empty()) {
        req::shared::logInfo("zone", std::string{"[GROUP] Group empty, disbanding groupId="} + 
            std::to_string(groupId));
        groups_.erase(groupId);
        return true;
    }
    
    // If the removed character was leader, promote first remaining member
    if (group->leaderCharacterId == characterId) {
        group->leaderCharacterId = members[0];
        req::shared::logInfo("zone", std::string{"[GROUP] New leader="} + 
            std::to_string(members[0]) + " for groupId=" + std::to_string(groupId));
    }
    
    return true;
}

void ZoneServer::disbandGroup(std::uint64_t groupId) {
    auto* group = getGroupById(groupId);
    if (!group) {
        req::shared::logWarn("zone", std::string{"[GROUP] Disband failed: group not found, groupId="} + 
            std::to_string(groupId));
        return;
    }
    
    // Remove all character mappings
    for (std::uint64_t memberId : group->memberCharacterIds) {
        characterToGroupId_.erase(memberId);
    }
    
    req::shared::logInfo("zone", std::string{"[GROUP] Disbanded groupId="} + std::to_string(groupId));
    
    // Remove group
    groups_.erase(groupId);
}

// ============================================================================
// High-Level Group Operations
// ============================================================================

void ZoneServer::handleGroupInvite(std::uint64_t inviterCharId, const std::string& targetName) {
    // Find target player by name in this zone
    ZonePlayer* targetPlayer = nullptr;
    for (auto& [charId, player] : players_) {
        if (player.isInitialized) {
            // Load character to get name
            auto character = characterStore_.loadById(charId);
            if (character && character->name == targetName) {
                targetPlayer = &player;
                break;
            }
        }
    }
    
    if (!targetPlayer) {
        req::shared::logWarn("zone", std::string{"[GROUP] Invite failed: target not found, name="} + targetName);
        return;
    }
    
    // Check if inviter is in a group
    auto* inviterGroup = getGroupForCharacter(inviterCharId);
    
    if (!inviterGroup) {
        // Create new group with inviter as leader
        inviterGroup = &createGroup(inviterCharId);
    } else {
        // Check if inviter is the leader
        if (inviterGroup->leaderCharacterId != inviterCharId) {
            req::shared::logWarn("zone", std::string{"[GROUP] Invite failed: not group leader, inviter="} + 
                std::to_string(inviterCharId));
            return;
        }
        
        // Check if group is full
        if (isGroupFull(*inviterGroup)) {
            req::shared::logWarn("zone", std::string{"[GROUP] Invite failed: group full, groupId="} + 
                std::to_string(inviterGroup->groupId));
            return;
        }
    }
    
    // For now, auto-accept and add to group
    // TODO: Implement proper invite/accept flow with pending invites
    if (addMemberToGroup(inviterGroup->groupId, targetPlayer->characterId)) {
        req::shared::logInfo("zone", std::string{"[GROUP] Invite accepted: groupId="} + 
            std::to_string(inviterGroup->groupId) + ", target=" + 
            std::to_string(targetPlayer->characterId));
    }
}

void ZoneServer::handleGroupAccept(std::uint64_t targetCharId, std::uint64_t groupId) {
    if (addMemberToGroup(groupId, targetCharId)) {
        req::shared::logInfo("zone", std::string{"[GROUP] Invite accepted: groupId="} + 
            std::to_string(groupId) + ", target=" + std::to_string(targetCharId));
    }
}

void ZoneServer::handleGroupDecline(std::uint64_t targetCharId, std::uint64_t groupId) {
    req::shared::logInfo("zone", std::string{"[GROUP] Invite declined: groupId="} + 
        std::to_string(groupId) + ", target=" + std::to_string(targetCharId));
}

void ZoneServer::handleGroupLeave(std::uint64_t characterId) {
    auto groupId = getCharacterGroup(characterId);
    if (!groupId.has_value()) {
        req::shared::logWarn("zone", std::string{"[GROUP] Leave failed: not in group, characterId="} + 
            std::to_string(characterId));
        return;
    }
    
    removeMemberFromGroup(*groupId, characterId);
    req::shared::logInfo("zone", std::string{"[GROUP] Character left: characterId="} + 
        std::to_string(characterId) + ", groupId=" + std::to_string(*groupId));
}

void ZoneServer::handleGroupKick(std::uint64_t leaderCharId, std::uint64_t targetCharId) {
    auto* group = getGroupForCharacter(leaderCharId);
    if (!group) {
        req::shared::logWarn("zone", std::string{"[GROUP] Kick failed: leader not in group, leaderCharId="} + 
            std::to_string(leaderCharId));
        return;
    }
    
    if (group->leaderCharacterId != leaderCharId) {
        req::shared::logWarn("zone", std::string{"[GROUP] Kick failed: not group leader, characterId="} + 
            std::to_string(leaderCharId));
        return;
    }
    
    if (removeMemberFromGroup(group->groupId, targetCharId)) {
        req::shared::logInfo("zone", std::string{"[GROUP] Kicked: leader="} + 
            std::to_string(leaderCharId) + ", target=" + std::to_string(targetCharId) + 
            ", groupId=" + std::to_string(group->groupId));
    }
}

void ZoneServer::handleGroupDisband(std::uint64_t leaderCharId) {
    auto* group = getGroupForCharacter(leaderCharId);
    if (!group) {
        req::shared::logWarn("zone", std::string{"[GROUP] Disband failed: not in group, leaderCharId="} + 
            std::to_string(leaderCharId));
        return;
    }
    
    if (group->leaderCharacterId != leaderCharId) {
        req::shared::logWarn("zone", std::string{"[GROUP] Disband failed: not group leader, characterId="} + 
            std::to_string(leaderCharId));
        return;
    }
    
    disbandGroup(group->groupId);
}

} // namespace req::zone
