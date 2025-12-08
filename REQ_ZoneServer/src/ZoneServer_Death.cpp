#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"
#include "../../REQ_Shared/include/req/shared/Config.h"

#include <chrono>
#include <algorithm>

namespace req::zone {

void ZoneServer::handlePlayerDeath(ZonePlayer& player) {
    if (player.isDead) {
        req::shared::logWarn("zone", std::string{"[DEATH] Player already dead: characterId="} +
            std::to_string(player.characterId));
        return;
    }
    
    req::shared::logInfo("zone", std::string{"[DEATH] ========== PLAYER DEATH BEGIN =========="});
    req::shared::logInfo("zone", std::string{"[DEATH] characterId="} + std::to_string(player.characterId));
    
    // Load character from disk
    auto character = characterStore_.loadById(player.characterId);
    if (!character.has_value()) {
        req::shared::logError("zone", std::string{"[DEATH] Cannot process death - character not found: characterId="} +
            std::to_string(player.characterId));
        return;
    }
    
    // Store old values for logging
    std::uint32_t oldLevel = character->level;
    std::uint64_t oldXp = character->xp;
    
    // Apply XP loss based on WorldRules
    // Rule: No XP loss below level 6 (from GDD)
    if (character->level >= 6) {
        float xpLossMultiplier = worldRules_.death.xpLossMultiplier;
        
        // Calculate XP loss
        // Get XP for current level and next level
        std::int64_t xpCurrentLevel = req::shared::GetTotalXpForLevel(xpTable_, character->level);
        std::int64_t xpNextLevel = req::shared::GetTotalXpForLevel(xpTable_, character->level + 1);
        std::int64_t xpIntoLevel = character->xp - xpCurrentLevel;
        std::int64_t xpToLose = static_cast<std::int64_t>(xpIntoLevel * xpLossMultiplier);
        
        // Ensure we don't lose more XP than we have in this level
        xpToLose = std::min(xpToLose, xpIntoLevel);
        
        // Apply XP loss
        character->xp -= xpToLose;
        
        // Check if we need to de-level
        while (character->level > 1 && character->xp < static_cast<std::uint64_t>(xpCurrentLevel)) {
            --character->level;
            xpCurrentLevel = req::shared::GetTotalXpForLevel(xpTable_, character->level);
            
            req::shared::logInfo("zone", std::string{"[DEATH] De-leveled: "} + 
                std::to_string(character->level + 1) + " -> " + std::to_string(character->level));
        }
        
        req::shared::logInfo("zone", std::string{"[DEATH] XP loss applied: characterId="} +
            std::to_string(player.characterId) + ", level=" + std::to_string(oldLevel) +
            " -> " + std::to_string(character->level) + ", xp=" + std::to_string(oldXp) +
            " -> " + std::to_string(character->xp) + " (lost " + std::to_string(xpToLose) + ")");
    } else {
        req::shared::logInfo("zone", std::string{"[DEATH] No XP loss - level "} +
            std::to_string(character->level) + " < 6 (safe from XP penalty)");
    }
    
    // Create corpse (if corpse runs enabled)
    if (worldRules_.death.corpseRunEnabled) {
        req::shared::data::Corpse corpse;
        corpse.corpseId = nextCorpseId_++;
        corpse.ownerCharacterId = player.characterId;
        corpse.worldId = worldId_;
        corpse.zoneId = zoneId_;
        corpse.posX = player.posX;
        corpse.posY = player.posY;
        corpse.posZ = player.posZ;
        
        // Calculate expiry time
        auto now = std::chrono::system_clock::now();
        auto expiryTime = now + std::chrono::minutes(worldRules_.death.corpseDecayMinutes);
        
        corpse.createdAtUnix = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        corpse.expiresAtUnix = std::chrono::duration_cast<std::chrono::seconds>(expiryTime.time_since_epoch()).count();
        
        corpses_[corpse.corpseId] = corpse;
        
        req::shared::logInfo("zone", std::string{"[DEATH] Corpse created: corpseId="} +
            std::to_string(corpse.corpseId) + ", owner=" + std::to_string(corpse.ownerCharacterId) +
            ", pos=(" + std::to_string(corpse.posX) + "," + std::to_string(corpse.posY) + "," +
            std::to_string(corpse.posZ) + "), expiresIn=" + std::to_string(worldRules_.death.corpseDecayMinutes) + "min");
    } else {
        req::shared::logInfo("zone", "[DEATH] Corpse runs disabled - no corpse created");
    }
    
    // Mark player as dead
    player.isDead = true;
    player.hp = 0;
    
    // Update ZonePlayer state from character
    player.level = character->level;
    player.xp = character->xp;
    player.combatStatsDirty = true;
    
    // Save character immediately
    if (characterStore_.saveCharacter(*character)) {
        req::shared::logInfo("zone", "[DEATH] Character saved successfully");
    } else {
        req::shared::logError("zone", "[DEATH] Failed to save character");
    }
    
    req::shared::logInfo("zone", std::string{"[DEATH] ========== PLAYER DEATH END =========="});
}

void ZoneServer::respawnPlayer(ZonePlayer& player) {
    if (!player.isDead) {
        req::shared::logWarn("zone", std::string{"[RESPAWN] Player not dead: characterId="} +
            std::to_string(player.characterId));
        return;
    }
    
    req::shared::logInfo("zone", std::string{"[RESPAWN] ========== PLAYER RESPAWN BEGIN =========="});
    req::shared::logInfo("zone", std::string{"[RESPAWN] characterId="} + std::to_string(player.characterId));
    
    // Load character to get bind point
    auto character = characterStore_.loadById(player.characterId);
    if (!character.has_value()) {
        req::shared::logError("zone", std::string{"[RESPAWN] Cannot respawn - character not found: characterId="} +
            std::to_string(player.characterId));
        return;
    }
    
    // Determine respawn location
    float respawnX, respawnY, respawnZ;
    bool useBindPoint = false;
    
    // Check if character has a valid bind point
    if (character->bindWorldId >= 0 && character->bindZoneId >= 0) {
        // Check if bind point is in this zone
        if (character->bindWorldId == static_cast<std::int32_t>(worldId_) &&
            character->bindZoneId == static_cast<std::int32_t>(zoneId_)) {
            // Respawn at bind point
            respawnX = character->bindX;
            respawnY = character->bindY;
            respawnZ = character->bindZ;
            useBindPoint = true;
            
            req::shared::logInfo("zone", std::string{"[RESPAWN] Using bind point in current zone: ("} +
                std::to_string(respawnX) + "," + std::to_string(respawnY) + "," +
                std::to_string(respawnZ) + ")");
        } else {
            // Bind point is in a different zone
            req::shared::logWarn("zone", std::string{"[RESPAWN] Bind point is in different zone (world="} +
                std::to_string(character->bindWorldId) + ", zone=" + std::to_string(character->bindZoneId) +
                ") - using current zone safe spawn (TODO: cross-zone respawn)");
            respawnX = zoneConfig_.safeX;
            respawnY = zoneConfig_.safeY;
            respawnZ = zoneConfig_.safeZ;
        }
    } else {
        // No bind point set - use zone safe spawn
        req::shared::logInfo("zone", "[RESPAWN] No bind point set - using zone safe spawn");
        respawnX = zoneConfig_.safeX;
        respawnY = zoneConfig_.safeY;
        respawnZ = zoneConfig_.safeZ;
    }
    
    // Move player to respawn location
    player.posX = respawnX;
    player.posY = respawnY;
    player.posZ = respawnZ;
    player.velX = 0.0f;
    player.velY = 0.0f;
    player.velZ = 0.0f;
    
    // Restore HP and mana (full restore for now - can be adjusted based on rules)
    player.hp = player.maxHp;
    player.mana = player.maxMana;
    
    // Clear death flag
    player.isDead = false;
    
    // Mark as dirty for save
    player.combatStatsDirty = true;
    player.isDirty = true;
    
    req::shared::logInfo("zone", std::string{"[RESPAWN] Player respawned: characterId="} +
        std::to_string(player.characterId) + ", pos=(" + std::to_string(respawnX) + "," +
        std::to_string(respawnY) + "," + std::to_string(respawnZ) + "), hp=" +
        std::to_string(player.hp) + "/" + std::to_string(player.maxHp) + ", mana=" +
        std::to_string(player.mana) + "/" + std::to_string(player.maxMana));
    
    req::shared::logInfo("zone", std::string{"[RESPAWN] ========== PLAYER RESPAWN END =========="});
}

void ZoneServer::processCorpseDecay() {
    auto now = std::chrono::system_clock::now();
    auto nowUnix = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    // Find expired corpses
    std::vector<std::uint64_t> expiredCorpses;
    for (const auto& [corpseId, corpse] : corpses_) {
        if (nowUnix >= corpse.expiresAtUnix) {
            expiredCorpses.push_back(corpseId);
        }
    }
    
    // Remove expired corpses
    for (std::uint64_t corpseId : expiredCorpses) {
        const auto& corpse = corpses_[corpseId];
        req::shared::logInfo("zone", std::string{"[CORPSE] Decayed: corpseId="} +
            std::to_string(corpseId) + ", owner=" + std::to_string(corpse.ownerCharacterId));
        corpses_.erase(corpseId);
    }
}

// Dev command implementations

void ZoneServer::devGiveXp(std::uint64_t characterId, std::int64_t amount) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        req::shared::logWarn("zone", std::string{"[DEV] GiveXP failed - player not found: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    ZonePlayer& player = playerIt->second;
    
    // Load character
    auto character = characterStore_.loadById(characterId);
    if (!character.has_value()) {
        req::shared::logError("zone", std::string{"[DEV] GiveXP failed - character not found: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    std::uint32_t oldLevel = character->level;
    std::uint64_t oldXp = character->xp;
    
    // Use AddXp helper to handle level-ups
    req::shared::AddXp(*character, amount, xpTable_, worldRules_);
    
    // Update ZonePlayer state
    player.level = character->level;
    player.xp = character->xp;
    player.combatStatsDirty = true;
    
    // Save character
    characterStore_.saveCharacter(*character);
    
    req::shared::logInfo("zone", std::string{"[DEV] GiveXP: characterId="} + std::to_string(characterId) +
        ", amount=" + std::to_string(amount) + ", level=" + std::to_string(oldLevel) +
        " -> " + std::to_string(character->level) + ", xp=" + std::to_string(oldXp) +
        " -> " + std::to_string(character->xp));
}

void ZoneServer::devSetLevel(std::uint64_t characterId, std::uint32_t level) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        req::shared::logWarn("zone", std::string{"[DEV] SetLevel failed - player not found: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    ZonePlayer& player = playerIt->second;
    
    // Load character
    auto character = characterStore_.loadById(characterId);
    if (!character.has_value()) {
        req::shared::logError("zone", std::string{"[DEV] SetLevel failed - character not found: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    // Clamp level to table range
    const int maxLevel = xpTable_.entries.empty() ? 50 : xpTable_.entries.back().level;
    level = std::max(1u, std::min(level, static_cast<std::uint32_t>(maxLevel)));
    
    std::uint32_t oldLevel = character->level;
    std::uint64_t oldXp = character->xp;
    
    // Set level and XP
    character->level = level;
    character->xp = req::shared::GetTotalXpForLevel(xpTable_, level);
    
    // Update ZonePlayer state
    player.level = character->level;
    player.xp = character->xp;
    player.combatStatsDirty = true;
    
    // Save character
    characterStore_.saveCharacter(*character);
    
    req::shared::logInfo("zone", std::string{"[DEV] SetLevel: characterId="} + std::to_string(characterId) +
        ", level=" + std::to_string(oldLevel) + " -> " + std::to_string(level) +
        ", xp=" + std::to_string(oldXp) + " -> " + std::to_string(character->xp));
}

void ZoneServer::devSuicide(std::uint64_t characterId) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        req::shared::logWarn("zone", std::string{"[DEV] Suicide failed - player not found: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    ZonePlayer& player = playerIt->second;
    
    if (player.isDead) {
        req::shared::logWarn("zone", std::string{"[DEV] Suicide failed - player already dead: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    req::shared::logInfo("zone", std::string{"[DEV] Suicide command: characterId="} + std::to_string(characterId));
    
    // Set HP to 0 and trigger death
    player.hp = 0;
    handlePlayerDeath(player);
}

void ZoneServer::devDamageSelf(std::uint64_t characterId, std::int32_t amount) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        req::shared::logWarn("zone", std::string{"[DEV] damage_self failed - player not found: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    ZonePlayer& player = playerIt->second;
    
    if (amount <= 0) {
        req::shared::logWarn("zone", std::string{"[DEV] damage_self failed - invalid amount: "} +
            std::to_string(amount));
        return;
    }
    
    std::int32_t oldHp = player.hp;
    std::int32_t newHp = std::max(0, oldHp - amount);
    player.hp = newHp;
    player.combatStatsDirty = true;
    
    req::shared::logInfo("zone", std::string{"[DEV] damage_self: characterId="} +
        std::to_string(characterId) + ", amount=" + std::to_string(amount) +
        ", hp " + std::to_string(oldHp) + " -> " + std::to_string(newHp));
    
    // If HP reached 0, trigger death
    if (newHp <= 0) {
        req::shared::logInfo("zone", std::string{"[DEV] damage_self killed player: characterId="} +
            std::to_string(characterId));
        handlePlayerDeath(player);
    }
}

// ============================================================================
// GM / Admin Commands for NPC Management
// ============================================================================

void ZoneServer::adminSpawnNpc(std::uint64_t gmCharacterId, std::int32_t npcId) {
    auto gmIt = players_.find(gmCharacterId);
    if (gmIt == players_.end()) {
        req::shared::logWarn("zone", std::string{"[ADMIN] admin_spawn_npc failed - GM not found: characterId="} +
            std::to_string(gmCharacterId));
        return;
    }

    const ZonePlayer& gm = gmIt->second;

    // Get NPC template
    const NpcTemplateData* tmpl = npcDataRepository_.GetTemplate(npcId);
    if (!tmpl) {
        req::shared::logWarn("zone", std::string{"[ADMIN] admin_spawn_npc failed - unknown NPC template: npcId="} +
            std::to_string(npcId));
        return;
    }

    // Create runtime NPC instance at GM's current position
    req::shared::data::ZoneNpc npc;

    // Generate unique instance ID
    npc.npcId = nextNpcInstanceId_++;

    // Copy data from template
    npc.name = tmpl->name;
    npc.level = tmpl->level;
    npc.templateId = tmpl->npcId;
    npc.spawnId = -1;  // -1 indicates admin-spawned (not from spawn table)
    npc.factionId = tmpl->factionId;

    // Set stats from template
    npc.maxHp = tmpl->hp;
    npc.currentHp = npc.maxHp;
    npc.isAlive = true;
    npc.minDamage = tmpl->minDamage;
    npc.maxDamage = tmpl->maxDamage;

    // Position at GM's location
    npc.posX = gm.posX;
    npc.posY = gm.posY;
    npc.posZ = gm.posZ;
    npc.facingDegrees = gm.yawDegrees;

    // Store spawn point (same as current position for admin spawns)
    npc.spawnX = gm.posX;
    npc.spawnY = gm.posY;
    npc.spawnZ = gm.posZ;

    // No automatic respawn for admin-spawned NPCs
    npc.respawnTimeSec = 0.0f;
    npc.respawnTimerSec = 0.0f;
    npc.pendingRespawn = false;

    // Behavior from template
    npc.behaviorFlags.isSocial = tmpl->isSocial;
    npc.behaviorFlags.canFlee = tmpl->canFlee;
    npc.behaviorFlags.isRoamer = tmpl->isRoamer;
    npc.behaviorFlags.leashToSpawn = true;

    npc.behaviorParams.aggroRadius = tmpl->aggroRadius * 10.0f;
    npc.behaviorParams.socialRadius = tmpl->assistRadius * 10.0f;
    npc.behaviorParams.leashRadius = 2000.0f;
    npc.behaviorParams.maxChaseDistance = 2500.0f;
    npc.behaviorParams.preferredRange = 200.0f;
    npc.behaviorParams.fleeHealthPercent = tmpl->canFlee ? 0.25f : 0.0f;

    // AI state
    npc.aiState = req::shared::data::NpcAiState::Idle;
    npc.currentTargetId = 0;
    npc.hateTable.clear();

    // Attack timing
    npc.meleeAttackCooldown = 1.5f;
    npc.meleeAttackTimer = 0.0f;
    npc.aggroScanTimer = 0.0f;
    npc.leashTimer = 0.0f;

    // Movement
    npc.moveSpeed = 50.0f;

    // Add to zone
    npcs_[npc.npcId] = npc;

    req::shared::logInfo("zone", std::string{"[ADMIN] Spawned NPC: instanceId="} +
        std::to_string(npc.npcId) + ", templateId=" + std::to_string(npc.templateId) +
        ", name=\"" + npc.name + "\", level=" + std::to_string(npc.level) +
        ", pos=(" + std::to_string(npc.posX) + "," + std::to_string(npc.posY) + "," +
        std::to_string(npc.posZ) + "), gmCharId=" + std::to_string(gmCharacterId));
}

} // namespace req::zone
