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
        if (target.level > 0) {
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
            
            // Calculate final XP
            float xpFloat = baseXp * levelModifier * xpRate * hotZoneMult;
            if (xpFloat < 1.0f) {
                xpFloat = 1.0f;
            }
            
            std::int64_t xpReward = static_cast<std::int64_t>(xpFloat);
            
            // Store old values for logging
            std::uint32_t oldLevel = attacker.level;
            std::uint64_t oldXp = attacker.xp;
            
            // Load character for XP addition
            auto character = characterStore_.loadById(attacker.characterId);
            if (character) {
                // Use AddXp helper to handle level-ups
                req::shared::AddXp(*character, xpReward, xpTable_, worldRules_);
                
                // Update ZonePlayer state from character
                attacker.level = character->level;
                attacker.xp = character->xp;
                attacker.combatStatsDirty = true;
                
                // Save character immediately
                characterStore_.saveCharacter(*character);
                
                req::shared::logInfo("zone", std::string{"[COMBAT][XP] killer="} +
                    std::to_string(attacker.characterId) + ", npc=" + std::to_string(target.npcId) +
                    ", npcLevel=" + std::to_string(target.level) + ", baseXp=" + std::to_string(static_cast<int>(baseXp)) +
                    ", finalXp=" + std::to_string(xpReward) + ", level=" + std::to_string(attacker.level) +
                    ", totalXp=" + std::to_string(attacker.xp));
                
                // Check if leveled up
                if (attacker.level > oldLevel) {
                    req::shared::logInfo("zone", std::string{"[LEVELUP] Character "} +
                        std::to_string(attacker.characterId) + " leveled up: " +
                        std::to_string(oldLevel) + " -> " + std::to_string(attacker.level));
                }
            } else {
                req::shared::logWarn("zone", std::string{"[COMBAT][XP] Failed to load character "} +
                    std::to_string(attacker.characterId) + " for XP award");
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

} // namespace req::zone
