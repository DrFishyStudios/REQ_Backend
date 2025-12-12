#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

namespace req::zone {

void ZoneServer::spawnPlayer(req::shared::data::Character& character, ZonePlayer& player) {
    // Determine spawn position based on character's last known location
    bool useRestoredPosition = false;
    
    // Rule 1: If lastZoneId matches this zone AND position is valid (non-zero), restore position
    if (character.lastZoneId == zoneId_) {
        // Check if position is valid (not all zeros)
        bool hasValidPosition = (character.positionX != 0.0f || 
                                character.positionY != 0.0f || 
                                character.positionZ != 0.0f);
        
        if (hasValidPosition) {
            // Restore saved position
            player.posX = character.positionX;
            player.posY = character.positionY;
            player.posZ = character.positionZ;
            player.yawDegrees = character.heading;
            useRestoredPosition = true;
            
            req::shared::logInfo("zone", std::string{"[SPAWN] Restored position for characterId="} +
                std::to_string(character.characterId) + ": pos=(" +
                std::to_string(player.posX) + "," + std::to_string(player.posY) + "," +
                std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));
        }
    }
    
    // Rule 2: Otherwise, use zone safe spawn point
    if (!useRestoredPosition) {
        player.posX = zoneConfig_.safeX;
        player.posY = zoneConfig_.safeY;
        player.posZ = zoneConfig_.safeZ;
        player.yawDegrees = zoneConfig_.safeYaw;
        
        req::shared::logInfo("zone", std::string{"[SPAWN] Using safe spawn point for characterId="} +
            std::to_string(character.characterId) + " (first visit or zone mismatch): pos=(" +
            std::to_string(player.posX) + "," + std::to_string(player.posY) + "," +
            std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));
        
        // Update character record to reflect new zone and position
        character.lastWorldId = worldId_;
        character.lastZoneId = zoneId_;
        character.positionX = player.posX;
        character.positionY = player.posY;
        character.positionZ = player.posZ;
        character.heading = player.yawDegrees;
        
        // Save updated character data
        if (characterStore_.saveCharacter(character)) {
            req::shared::logInfo("zone", std::string{"[SPAWN] Updated character lastZone/position: characterId="} +
                std::to_string(character.characterId) + ", lastZoneId=" + std::to_string(zoneId_));
        } else {
            req::shared::logWarn("zone", std::string{"[SPAWN] Failed to save character position: characterId="} +
                std::to_string(character.characterId));
        }
    }
    
    // Initialize velocity
    player.velX = 0.0f;
    player.velY = 0.0f;
    player.velZ = 0.0f;
    
    // Initialize combat state from character
    player.level = character.level;
    player.xp = character.xp;
    player.hp = (character.hp > 0) ? character.hp : character.maxHp;
    player.maxHp = character.maxHp;
    player.mana = character.mana;
    player.maxMana = character.maxMana;
    
    player.strength = character.strength;
    player.stamina = character.stamina;
    player.agility = character.agility;
    player.dexterity = character.dexterity;
    player.intelligence = character.intelligence;
    player.wisdom = character.wisdom;
    player.charisma = character.charisma;
    
    req::shared::logInfo("zone", std::string{"[SPAWN] Combat state initialized: characterId="} +
        std::to_string(character.characterId) + ", level=" + std::to_string(player.level) +
        ", xp=" + std::to_string(player.xp) + ", hp=" + std::to_string(player.hp) + "/" +
        std::to_string(player.maxHp));
}

void ZoneServer::setZoneConfig(const ZoneConfig& config) {
    zoneConfig_ = config;
    req::shared::logInfo("zone", std::string{"Zone config updated: safeSpawn=("} +
        std::to_string(config.safeX) + "," + std::to_string(config.safeY) + "," +
        std::to_string(config.safeZ) + "), safeYaw=" + std::to_string(config.moveSpeed) +
        ", moveSpeed=" + std::to_string(config.moveSpeed) +
        ", autosaveInterval=" + std::to_string(config.autosaveIntervalSec) + "s" +
        ", broadcastFullState=" + (config.broadcastFullState ? "true" : "false") +
        ", interestRadius=" + std::to_string(config.interestRadius) +
        ", debugInterest=" + (config.debugInterest ? "true" : "false"));
}

void ZoneServer::removePlayer(std::uint64_t characterId) {
    req::shared::logInfo("zone", std::string{"[REMOVE_PLAYER] BEGIN: characterId="} + 
        std::to_string(characterId));
    
    auto it = players_.find(characterId);
    if (it == players_.end()) {
        req::shared::logWarn("zone", std::string{"[REMOVE_PLAYER] Character not found in players map: characterId="} +
            std::to_string(characterId));
        req::shared::logInfo("zone", "[REMOVE_PLAYER] END (player not found)");
        return;
    }
    
    const ZonePlayer& player = it->second;
    
    req::shared::logInfo("zone", std::string{"[REMOVE_PLAYER] Found player: accountId="} +
        std::to_string(player.accountId) + ", pos=(" + std::to_string(player.posX) + "," +
        std::to_string(player.posY) + "," + std::to_string(player.posZ) + ")");
    
    // Save final position before removing (wrapped in try-catch)
    req::shared::logInfo("zone", "[REMOVE_PLAYER] Attempting to save character state...");
    try {
        savePlayerPosition(characterId);
        req::shared::logInfo("zone", "[REMOVE_PLAYER] Character state saved successfully");
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[REMOVE_PLAYER] Exception during savePlayerPosition: "} + 
            e.what());
        req::shared::logError("zone", "[REMOVE_PLAYER] Continuing with player removal despite save failure");
    } catch (...) {
        req::shared::logError("zone", "[REMOVE_PLAYER] Unknown exception during savePlayerPosition");
        req::shared::logError("zone", "[REMOVE_PLAYER] Continuing with player removal despite save failure");
    }
    
    // Remove from all NPC hate tables before removing from zone
    req::shared::logInfo("zone", "[REMOVE_PLAYER] Removing from all NPC hate tables");
    removeCharacterFromAllHateTables(player.characterId);
    
    // Remove from connection mapping (if connection still exists)
    if (player.connection) {
        auto connIt = connectionToCharacterId_.find(player.connection);
        if (connIt != connectionToCharacterId_.end()) {
            connectionToCharacterId_.erase(connIt);
            req::shared::logInfo("zone", "[REMOVE_PLAYER] Removed from connection mapping");
        }
    }
    
    // Remove from players map
    players_.erase(it);
    req::shared::logInfo("zone", "[REMOVE_PLAYER] Removed from players map");
    
    req::shared::logInfo("zone", std::string{"[REMOVE_PLAYER] END: characterId="} +
        std::to_string(characterId) + ", remaining_players=" + std::to_string(players_.size()));
}

} // namespace req::zone
