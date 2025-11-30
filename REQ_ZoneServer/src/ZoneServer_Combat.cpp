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
    
    if (target.currentHp <= 0) {
        target.currentHp = 0;
        target.isAlive = false;
        targetDied = true;
        
        req::shared::logInfo("zone", std::string{"[COMBAT] NPC slain: npcId="} +
            std::to_string(target.npcId) + ", name=\"" + target.name + 
            "\", killerCharId=" + std::to_string(attacker.characterId));
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
