#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

#include <random>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace req::zone {

void ZoneServer::processAttack(ZonePlayer& attacker, req::shared::data::ZoneNpc& target,
                               const req::shared::protocol::AttackRequestData& request) {
    // Check if attacker is dead
    if (attacker.isDead) {
        req::shared::protocol::AttackResultData result;
        result.attackerId = attacker.characterId;
        result.targetId = target.npcId;
        result.damage = 0;
        result.wasHit = false;
        result.remainingHp = 0;
        result.resultCode = 6; // Attacker is dead
        result.message = "You cannot attack while dead";
        
        broadcastAttackResult(result);
        return;
    }
    
    // Check if target is already dead
    if (!target.isAlive || target.currentHp <= 0) {
        req::shared::protocol::AttackResultData result;
        result.attackerId = attacker.characterId;
        result.targetId = target.npcId;
        result.damage = 0;
        result.wasHit = false;
        result.remainingHp = 0;
        result.resultCode = 5; // Target is dead
        result.message = target.name + " is already dead";
        
        broadcastAttackResult(result);
        return;
    }
    
    // Check range (simple Euclidean distance)
    float dx = attacker.posX - target.posX;
    float dy = attacker.posY - target.posY;
    float dz = attacker.posZ - target.posZ;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    
    constexpr float MAX_ATTACK_RANGE = 200.0f; // Configurable
    
    if (distance > MAX_ATTACK_RANGE) {
        req::shared::logWarn("zone", std::string{"[COMBAT] Out of range: distance="} +
            std::to_string(distance) + ", max=" + std::to_string(MAX_ATTACK_RANGE));
        
        req::shared::protocol::AttackResultData result;
        result.attackerId = attacker.characterId;
        result.targetId = target.npcId;
        result.damage = 0;
        result.wasHit = false;
        result.remainingHp = target.currentHp;
        result.resultCode = 1; // Out of range
        result.message = "Target out of range";
        
        broadcastAttackResult(result);
        return;
    }
    
    // Calculate hit chance (simple: 95% hit for now)
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> hitRoll(1, 100);
    bool didHit = (hitRoll(rng) <= 95);
    
    if (!didHit) {
        req::shared::logInfo("zone", std::string{"[COMBAT] Attack missed: attacker="} +
            std::to_string(attacker.characterId) + ", target=" + std::to_string(target.npcId));
        
        req::shared::protocol::AttackResultData result;
        result.attackerId = attacker.characterId;
        result.targetId = target.npcId;
        result.damage = 0;
        result.wasHit = false;
        result.remainingHp = target.currentHp;
        result.resultCode = 0; // Success (but missed)
        result.message = "You miss " + target.name;
        
        broadcastAttackResult(result);
        return;
    }
    
    // Calculate damage: base damage from level + strength bonus + random
    int baseDamage = 5 + (attacker.level * 2);
    int strengthBonus = attacker.strength / 10;
    std::uniform_int_distribution<int> damageVariance(-2, 5);
    int variance = damageVariance(rng);
    
    int totalDamage = baseDamage + strengthBonus + variance;
    totalDamage = std::max(1, totalDamage); // Minimum 1 damage
    
    // Apply damage to NPC
    target.currentHp -= totalDamage;
    bool targetDied = false;
    
    // Damage aggro: add hate for this attacker (Phase 2.3)
    constexpr float MELEE_HATE_SCALAR = 1.0f;  // Can be configurable per WorldRules
    addHate(target, attacker.characterId, static_cast<float>(totalDamage) * MELEE_HATE_SCALAR);
    
    // Trigger state transition if NPC is idle or alert
    using NpcAiState = req::shared::data::NpcAiState;
    if (target.aiState == NpcAiState::Idle || target.aiState == NpcAiState::Alert) {
        target.aiState = NpcAiState::Engaged;
        
        req::shared::logInfo("zone", std::string{"[AI] NPC "} + std::to_string(target.npcId) +
            " \"" + target.name + "\" state->Engaged (damage aggro)" +
            ", attacker=" + std::to_string(attacker.characterId) +
            ", damage=" + std::to_string(totalDamage));
    }
    
    if (target.currentHp <= 0) {
        target.currentHp = 0;
        target.isAlive = false;
        targetDied = true;
        
        req::shared::logInfo("zone", std::string{"[COMBAT] NPC slain: npcId="} +
            std::to_string(target.npcId) + ", name=\"" + target.name + 
            "\", killerCharId=" + std::to_string(attacker.characterId));
        
        // Award XP for kill
        if (targetDied) {
            // Award XP for kill
            awardXpForNpcKill(target, attacker);
            
            // CRITICAL: Broadcast EntityDespawn so clients remove dead NPC
            broadcastEntityDespawn(target.npcId, 1);  // Reason 1 = Death
            
            // Schedule respawn if NPC has a spawn point
            if (target.spawnId > 0) {
                auto now = std::chrono::system_clock::now();
                double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
                scheduleRespawn(target.spawnId, currentTime);
            }
        }
    }
    
    // Build result message
    std::ostringstream msgBuilder;
    if (targetDied) {
        msgBuilder << "You hit " << target.name << " for " << totalDamage 
                   << " points of damage. " << target.name << " has been slain!";
    } else {
        msgBuilder << "You hit " << target.name << " for " << totalDamage << " points of damage";
    }
    
    req::shared::logInfo("zone", std::string{"[COMBAT] Attack hit: attacker="} +
        std::to_string(attacker.characterId) + ", target=" + std::to_string(target.npcId) +
        ", damage=" + std::to_string(totalDamage) + ", remainingHp=" + std::to_string(target.currentHp));
    
    // Build and broadcast result
    req::shared::protocol::AttackResultData result;
    result.attackerId = attacker.characterId;
    result.targetId = target.npcId;
    result.damage = totalDamage;
    result.wasHit = true;
    result.remainingHp = target.currentHp;
    result.resultCode = 0; // Success
    result.message = msgBuilder.str();
    
    broadcastAttackResult(result);
}

void ZoneServer::broadcastAttackResult(const req::shared::protocol::AttackResultData& result) {
    std::string payload = req::shared::protocol::buildAttackResultPayload(result);
    req::shared::net::Connection::ByteArray payloadBytes(payload.begin(), payload.end());
    
    req::shared::logInfo("zone", std::string{"[COMBAT] AttackResult: attacker="} +
        std::to_string(result.attackerId) + ", target=" + std::to_string(result.targetId) +
        ", dmg=" + std::to_string(result.damage) + ", hit=" + (result.wasHit ? "1" : "0") +
        ", remainingHp=" + std::to_string(result.remainingHp) +
        ", resultCode=" + std::to_string(result.resultCode) +
        ", msg=\"" + result.message + "\"");
    
    // Broadcast to all clients in the zone (for now - could optimize to nearby only)
    int sentCount = 0;
    for (auto& connection : connections_) {
        if (!connection) {
            continue;
        }
        
        // Check if connection is closed
        if (connection->isClosed()) {
            continue;
        }
        
        try {
            connection->send(req::shared::MessageType::AttackResult, payloadBytes);
            sentCount++;
        } catch (const std::exception& e) {
            req::shared::logWarn("zone", std::string{"[COMBAT] Failed to send AttackResult to connection: "} + e.what());
        }
    }
    
    req::shared::logInfo("zone", std::string{"[COMBAT] AttackResult broadcasted to "} + std::to_string(sentCount) + " connection(s)");
}

void ZoneServer::awardXpForNpcKill(req::shared::data::ZoneNpc& target, ZonePlayer& attacker) {
    if (target.level <= 0) {
        return;
    }
    
    // Calculate base XP from target level
    float baseXp = 10.0f * static_cast<float>(target.level);
    
    // Apply level difference modifier (con-based)
    int levelDiff = target.level - attacker.level;
    float levelModifier = 1.0f;
    
    if (levelDiff >= 3) {
        levelModifier = 1.5f;   // red con
    } else if (levelDiff >= 1) {
        levelModifier = 1.2f;   // yellow
    } else if (levelDiff <= -3) {
        levelModifier = 0.25f;  // gray (trivial)
    } else if (levelDiff <= -1) {
        levelModifier = 0.5f;   // blue/green
    }
    
    // Apply WorldRules XP base rate
    float xpRate = worldRules_.xp.baseRate;
    if (xpRate < 0.0f) {
        xpRate = 0.0f;
    }
    
    // Check for hot zone multiplier
    float hotZoneMult = 1.0f;
    for (const auto& hz : worldRules_.hotZones) {
        if (hz.zoneId == zoneConfig_.zoneId) {
            if (hz.xpMultiplier > 0.0f) {
                hotZoneMult = hz.xpMultiplier;
            }
            break;
        }
    }
    
    // Calculate base XP with modifiers (before group consideration)
    float baseXpWithMods = baseXp * levelModifier * xpRate * hotZoneMult;
    
    // Check if killer is in a group
    auto* group = getGroupForCharacter(attacker.characterId);
    
    if (!group) {
        // Solo kill - award all XP to killer
        if (baseXpWithMods < 1.0f) {
            baseXpWithMods = 1.0f;
        }
        
        std::int64_t xpReward = static_cast<std::int64_t>(baseXpWithMods);
        
        auto character = characterStore_.loadById(attacker.characterId);
        if (character) {
            std::uint32_t oldLevel = character->level;
            std::uint64_t oldXp = character->xp;
            
            req::shared::AddXp(*character, xpReward, xpTable_, worldRules_);
            
            attacker.level = character->level;
            attacker.xp = character->xp;
            attacker.combatStatsDirty = true;
            
            characterStore_.saveCharacter(*character);
            
            req::shared::logInfo("zone", std::string{"[COMBAT][XP] Solo kill: killer="} +
                std::to_string(attacker.characterId) + ", npc=" + std::to_string(target.npcId) +
                ", npcLevel=" + std::to_string(target.level) + ", baseXp=" + std::to_string(static_cast<int>(baseXp)) +
                ", finalXp=" + std::to_string(xpReward) + ", level=" + std::to_string(attacker.level) +
                ", totalXp=" + std::to_string(attacker.xp));
            
            if (attacker.level > oldLevel) {
                req::shared::logInfo("zone", std::string{"[LEVELUP] Character "} +
                    std::to_string(attacker.characterId) + " leveled up: " +
                    std::to_string(oldLevel) + " -> " + std::to_string(attacker.level));
            }
        }
    } else {
        // Group kill - distribute XP among eligible members
        constexpr float kMaxGroupXpRange = 4000.0f; // Max distance for XP sharing
        
        // Build list of eligible members
        std::vector<std::uint64_t> eligibleMembers;
        for (std::uint64_t memberId : group->memberCharacterIds) {
            auto memberIt = players_.find(memberId);
            if (memberIt == players_.end()) {
                continue; // Not in this zone
            }
            
            ZonePlayer& member = memberIt->second;
            if (!member.isInitialized || member.isDead) {
                continue; // Not alive or not initialized
            }
            
            // Check distance from kill location
            float dx = member.posX - target.posX;
            float dy = member.posY - target.posY;
            float dz = member.posZ - target.posZ;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            
            if (distance <= kMaxGroupXpRange) {
                eligibleMembers.push_back(memberId);
            }
        }
        
        if (eligibleMembers.empty()) {
            req::shared::logWarn("zone", std::string{"[XP][Group] No eligible members for XP, groupId="} +
                std::to_string(group->groupId));
            return;
        }
        
        // Apply group bonus from WorldRules
        const std::size_t eligibleCount = eligibleMembers.size();
        double bonusFactor = 1.0;
        if (eligibleCount > 1) {
            bonusFactor += worldRules_.xp.groupBonusPerMember * static_cast<double>(eligibleCount - 1);
        }
        
        const std::int64_t xpPool = static_cast<std::int64_t>(std::round(baseXpWithMods * bonusFactor));
        
        // Split XP evenly among eligible members
        const std::int64_t share = xpPool / static_cast<std::int64_t>(eligibleCount);
        
        // Log group XP distribution
        std::ostringstream memberIds;
        for (size_t i = 0; i < eligibleMembers.size(); ++i) {
            if (i > 0) memberIds << ",";
            memberIds << eligibleMembers[i];
        }
        
        req::shared::logInfo("zone", std::string{"[XP][Group] npc="} + std::to_string(target.npcId) +
            ", base=" + std::to_string(static_cast<int>(baseXpWithMods)) +
            ", pool=" + std::to_string(xpPool) +
            ", members=" + memberIds.str() +
            ", share=" + std::to_string(share));
        
        // Award XP to each eligible member
        for (std::uint64_t memberId : eligibleMembers) {
            auto character = characterStore_.loadById(memberId);
            if (!character) {
                continue;
            }
            
            std::uint32_t oldLevel = character->level;
            std::uint64_t oldXp = character->xp;
            
            req::shared::AddXp(*character, share, xpTable_, worldRules_);
            
            // Update ZonePlayer state
            auto memberIt = players_.find(memberId);
            if (memberIt != players_.end()) {
                memberIt->second.level = character->level;
                memberIt->second.xp = character->xp;
                memberIt->second.combatStatsDirty = true;
            }
            
            characterStore_.saveCharacter(*character);
            
            req::shared::logInfo("zone", std::string{"[XP][Group] Member "} + std::to_string(memberId) +
                " awarded " + std::to_string(share) + " XP, level=" + std::to_string(character->level) +
                ", totalXp=" + std::to_string(character->xp));
            
            if (character->level > oldLevel) {
                req::shared::logInfo("zone", std::string{"[LEVELUP] Character "} +
                    std::to_string(memberId) + " leveled up: " +
                    std::to_string(oldLevel) + " -> " + std::to_string(character->level));
            }
        }
    }
}

} // namespace req::zone
