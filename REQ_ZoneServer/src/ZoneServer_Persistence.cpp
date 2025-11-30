#include "../include/req/zone/ZoneServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/DataModels.h"

namespace req::zone {

void ZoneServer::savePlayerPosition(std::uint64_t characterId) {
    auto playerIt = players_.find(characterId);
    if (playerIt == players_.end()) {
        req::shared::logWarn("zone", std::string{"[SAVE] Player not found in map: characterId="} +
            std::to_string(characterId));
        return;
    }
    
    const ZonePlayer& player = playerIt->second;
    
    // Wrap entire save operation in try-catch
    try {
        // Load character from disk
        auto character = characterStore_.loadById(characterId);
        if (!character.has_value()) {
            req::shared::logWarn("zone", std::string{"[SAVE] Cannot save position - character not found on disk: characterId="} +
                std::to_string(characterId));
            return;
        }
        
        // Update position fields
        character->lastWorldId = worldId_;
        character->lastZoneId = zoneId_;
        character->positionX = player.posX;
        character->positionY = player.posY;
        character->positionZ = player.posZ;
        character->heading = player.yawDegrees;
        
        // Update combat state if dirty
        if (player.combatStatsDirty) {
            character->level = player.level;
            character->hp = player.hp;
            character->maxHp = player.maxHp;
            character->mana = player.mana;
            character->maxMana = player.maxMana;
            
            character->strength = player.strength;
            character->stamina = player.stamina;
            character->agility = player.agility;
            character->dexterity = player.dexterity;
            character->intelligence = player.intelligence;
            character->wisdom = player.wisdom;
            character->charisma = player.charisma;
            
            req::shared::logInfo("zone", std::string{"[SAVE] Combat stats saved: characterId="} +
                std::to_string(characterId) + ", hp=" + std::to_string(player.hp) + "/" +
                std::to_string(player.maxHp) + ", mana=" + std::to_string(player.mana) + "/" +
                std::to_string(player.maxMana));
        }
        
        // Save to disk
        if (characterStore_.saveCharacter(*character)) {
            req::shared::logInfo("zone", std::string{"[SAVE] Position saved successfully: characterId="} +
                std::to_string(characterId) + ", zoneId=" + std::to_string(zoneId_) +
                ", pos=(" + std::to_string(player.posX) + "," + std::to_string(player.posY) + "," +
                std::to_string(player.posZ) + "), yaw=" + std::to_string(player.yawDegrees));
            
            // Mark as clean after successful save
            playerIt->second.isDirty = false;
            playerIt->second.combatStatsDirty = false;
        } else {
            req::shared::logError("zone", std::string{"[SAVE] Failed to save character to disk: characterId="} +
                std::to_string(characterId));
        }
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[SAVE] Exception during save: characterId="} +
            std::to_string(characterId) + ", error: " + e.what());
        // Don't rethrow - just log and return
    } catch (...) {
        req::shared::logError("zone", std::string{"[SAVE] Unknown exception during save: characterId="} +
            std::to_string(characterId));
        // Don't rethrow - just log and return
    }
}

void ZoneServer::saveAllPlayerPositions() {
    int savedCount = 0;
    int skippedCount = 0;
    int failedCount = 0;
    
    req::shared::logInfo("zone", "[AUTOSAVE] Beginning autosave of dirty player positions");
    
    for (auto& [characterId, player] : players_) {
        if (!player.isInitialized) {
            skippedCount++;
            continue;
        }
        
        if (!player.isDirty && !player.combatStatsDirty) {
            skippedCount++;
            continue;
        }
        
        try {
            savePlayerPosition(characterId);
            savedCount++;
        } catch (const std::exception& e) {
            req::shared::logError("zone", std::string{"[AUTOSAVE] Exception during saveCharacter: characterId="} +
                std::to_string(characterId) + ": " + e.what());
            failedCount++;
        } catch (...) {
            req::shared::logError("zone", std::string{"[AUTOSAVE] Unknown exception during saveCharacter: characterId="} +
                std::to_string(characterId));
            failedCount++;
        }
    }
    
    if (savedCount > 0 || failedCount > 0) {
        req::shared::logInfo("zone", std::string{"[AUTOSAVE] Complete: saved="} +
            std::to_string(savedCount) + ", skipped=" + std::to_string(skippedCount) +
            ", failed=" + std::to_string(failedCount));
    }
}

void ZoneServer::scheduleAutosave() {
    auto intervalMs = std::chrono::milliseconds(
        static_cast<int>(zoneConfig_.autosaveIntervalSec * 1000.0f));
    
    autosaveTimer_.expires_after(intervalMs);
    autosaveTimer_.async_wait([this](const boost::system::error_code& ec) {
        onAutosave(ec);
    });
}

void ZoneServer::onAutosave(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
        req::shared::logInfo("zone", "Autosave timer cancelled (server shutting down)");
        return;
    }
    
    if (ec) {
        req::shared::logError("zone", std::string{"Autosave timer error: "} + ec.message());
        // Continue with next autosave anyway
        scheduleAutosave();
        return;
    }
    
    // Save all dirty player positions (wrapped in try-catch)
    try {
        saveAllPlayerPositions();
    } catch (const std::exception& e) {
        req::shared::logError("zone", std::string{"[AUTOSAVE] Exception during autosave: "} + e.what());
    } catch (...) {
        req::shared::logError("zone", "[AUTOSAVE] Unknown exception during autosave");
    }
    
    // Always schedule next autosave, even if this one failed
    scheduleAutosave();
}

} // namespace req::zone
