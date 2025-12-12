#include "VizCombat.h"

#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>

// ============================================================================
// Helper: World to Screen Transform
// ============================================================================

namespace {
    sf::Vector2f WorldToScreen(
        float wx, float wy,
        sf::Vector2f cameraWorld,
        float pixelsPerWorldUnit,
        float windowWidth = 1280.0f,
        float windowHeight = 720.0f)
    {
        const float screenX = (windowWidth / 2.0f) + (wx - cameraWorld.x) * pixelsPerWorldUnit;
        const float screenY = (windowHeight / 2.0f) - (wy - cameraWorld.y) * pixelsPerWorldUnit;
        return sf::Vector2f{ screenX, screenY };
    }
    
    float DistanceSquared(sf::Vector2f a, sf::Vector2f b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }
}

// ============================================================================
// VizCombat_HandleMouseClickSelect
// ============================================================================

void VizCombat_HandleMouseClickSelect(
    VizCombatState& combat,
    const VizWorldState& worldState,
    sf::Vector2f mouseScreenPos,
    sf::Vector2f cameraWorld,
    float pixelsPerWorldUnit,
    float selectRadiusPx)
{
    const auto& entities = worldState.getEntities();
    
    // Find nearest entity within radius
    std::uint64_t nearestId = 0;
    float nearestDistSq = selectRadiusPx * selectRadiusPx;
    
    for (const auto& [id, entity] : entities) {
        // Skip local player
        if (entity.isLocalPlayer) {
            continue;
        }
        
        // Transform entity to screen space
        sf::Vector2f entityScreen = WorldToScreen(
            entity.posX, entity.posY,
            cameraWorld, pixelsPerWorldUnit);
        
        // Check distance to mouse
        float distSq = DistanceSquared(mouseScreenPos, entityScreen);
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearestId = id;
        }
    }
    
    // Update selection
    if (nearestId != 0) {
        combat.selectedTargetId = nearestId;
        
        // Find entity name for log
        auto it = entities.find(nearestId);
        if (it != entities.end()) {
            std::ostringstream oss;
            oss << "Target: " << (it->second.name.empty() ? "Entity" : it->second.name)
                << " (ID " << nearestId << ")";
            
            combat.combatLog.push_back(oss.str());
            if (combat.combatLog.size() > combat.MAX_LOG_LINES) {
                combat.combatLog.pop_front();
            }
            
            std::cout << "[COMBAT] " << oss.str() << "\n";
        }
    }
}

// ============================================================================
// VizCombat_HandleAttackKey
// ============================================================================

bool VizCombat_HandleAttackKey(
    VizCombatState& combat,
    const req::clientcore::ClientSession& session)
{
    // Check if target selected
    if (combat.selectedTargetId == 0) {
        std::cout << "[COMBAT] No target selected (click an entity first)\n";
        return false;
    }
    
    // Check cooldown
    if (combat.attackClock.getElapsedTime().asSeconds() < combat.attackCooldownSec) {
        return false; // Still on cooldown
    }
    
    // Send attack request
    const bool sent = req::clientcore::sendAttackRequest(
        session,
        combat.selectedTargetId,
        0,    // abilityId = 0 (basic attack)
        true  // isBasicAttack
    );
    
    if (sent) {
        combat.attackClock.restart();
        combat.attacksSent++;
        
        std::ostringstream oss;
        oss << "Attack sent -> " << combat.selectedTargetId;
        combat.combatLog.push_back(oss.str());
        if (combat.combatLog.size() > combat.MAX_LOG_LINES) {
            combat.combatLog.pop_front();
        }
        
        std::cout << "[COMBAT] " << oss.str() << "\n";
        return true;
    } else {
        std::cerr << "[COMBAT] Failed to send AttackRequest\n";
        return false;
    }
}

// ============================================================================
// VizCombat_HandleAttackResult
// ============================================================================

bool VizCombat_HandleAttackResult(
    VizCombatState& combat,
    const std::string& payload)
{
    req::shared::protocol::AttackResultData result;
    if (!req::clientcore::parseAttackResult(payload, result)) {
        std::cerr << "[COMBAT] Failed to parse AttackResult\n";
        return false;
    }
    
    combat.attacksReceived++;
    
    // Build combat log entry
    std::ostringstream oss;
    
    if (result.resultCode != 0) {
        // Error case
        oss << "Attack FAILED: " << result.message;
    } else if (result.wasHit) {
        // Hit
        oss << "HIT for " << result.damage << " dmg (HP: " << result.remainingHp << ")";
        if (result.remainingHp <= 0) {
            oss << " [DEAD]";
        }
    } else {
        // Miss
        oss << "MISS (no damage)";
    }
    
    combat.combatLog.push_back(oss.str());
    if (combat.combatLog.size() > combat.MAX_LOG_LINES) {
        combat.combatLog.pop_front();
    }
    
    std::cout << "[COMBAT] " << oss.str() << "\n";
    
    return true;
}

// ============================================================================
// VizCombat_DrawTargetIndicator
// ============================================================================

void VizCombat_DrawTargetIndicator(
    sf::RenderWindow& window,
    const VizCombatState& combat,
    const VizWorldState& worldState,
    sf::Vector2f cameraWorld,
    float pixelsPerWorldUnit)
{
    // Check if target selected
    if (combat.selectedTargetId == 0) {
        return;
    }
    
    // Find target entity
    const auto& entities = worldState.getEntities();
    auto it = entities.find(combat.selectedTargetId);
    if (it == entities.end()) {
        return; // Target no longer exists
    }
    
    const VizEntity& target = it->second;
    
    // Transform to screen space
    sf::Vector2f screenPos = WorldToScreen(
        target.posX, target.posY,
        cameraWorld, pixelsPerWorldUnit);
    
    // Draw yellow ring around target
    const float ringRadius = target.isNpc ? 10.0f : 12.0f;
    sf::CircleShape ring;
    ring.setRadius(ringRadius);
    ring.setOrigin({ ringRadius, ringRadius });
    ring.setPosition(screenPos);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(sf::Color(255, 255, 0, 200)); // Yellow
    ring.setOutlineThickness(2.0f);
    window.draw(ring);
    
    // Draw HP bar above target (if HP data available)
    if (target.maxHp > 0 && target.hp > 0) {
        const float barWidth = 40.0f;
        const float barHeight = 4.0f;
        const float barOffsetY = -20.0f;
        
        sf::Vector2f barPos{ screenPos.x - barWidth / 2.0f, screenPos.y + barOffsetY };
        
        // Background (black)
        sf::RectangleShape barBg({ barWidth, barHeight });
        barBg.setPosition(barPos);
        barBg.setFillColor(sf::Color(0, 0, 0, 180));
        window.draw(barBg);
        
        // HP fill (red to yellow to green)
        float hpPercent = static_cast<float>(target.hp) / static_cast<float>(target.maxHp);
        hpPercent = std::max(0.0f, std::min(1.0f, hpPercent));
        
        sf::Color hpColor;
        if (hpPercent > 0.5f) {
            hpColor = sf::Color::Green;
        } else if (hpPercent > 0.25f) {
            hpColor = sf::Color::Yellow;
        } else {
            hpColor = sf::Color::Red;
        }
        
        sf::RectangleShape barFill({ barWidth * hpPercent, barHeight });
        barFill.setPosition(barPos);
        barFill.setFillColor(hpColor);
        window.draw(barFill);
    }
}

// ============================================================================
// VizCombat_ClearTargetIfDespawned
// ============================================================================

void VizCombat_ClearTargetIfDespawned(
    VizCombatState& combat,
    const VizWorldState& worldState)
{
    if (combat.selectedTargetId == 0) {
        return; // No target selected
    }
    
    const auto& entities = worldState.getEntities();
    if (entities.find(combat.selectedTargetId) == entities.end()) {
        // Target despawned
        std::cout << "[COMBAT] Target " << combat.selectedTargetId << " despawned, clearing selection\n";
        
        combat.combatLog.push_back("Target despawned");
        if (combat.combatLog.size() > combat.MAX_LOG_LINES) {
            combat.combatLog.pop_front();
        }
        
        combat.selectedTargetId = 0;
    }
}

// ============================================================================
// VizCombat_CycleTarget
// ============================================================================

void VizCombat_CycleTarget(
    VizCombatState& combat,
    const VizWorldState& worldState,
    std::uint64_t localCharacterId,
    bool forward)
{
    const auto& entities = worldState.getEntities();
    
    // Find local player position
    sf::Vector2f localPos{ 0.0f, 0.0f };
    bool foundLocal = false;
    
    for (const auto& [id, entity] : entities) {
        if (id == localCharacterId) {
            localPos.x = entity.posX;
            localPos.y = entity.posY;
            foundLocal = true;
            break;
        }
    }
    
    if (!foundLocal) {
        std::cout << "[COMBAT] Cannot cycle targets: local player not found\n";
        return;
    }
    
    // Build list of targetable NPCs sorted by distance
    struct TargetCandidate {
        std::uint64_t id;
        float distanceSq;
    };
    
    std::vector<TargetCandidate> candidates;
    
    for (const auto& [id, entity] : entities) {
        if (entity.isLocalPlayer || !entity.isNpc) {
            continue; // Skip local player and other players
        }
        
        float dx = entity.posX - localPos.x;
        float dy = entity.posY - localPos.y;
        float distSq = dx * dx + dy * dy;
        
        candidates.push_back({ id, distSq });
    }
    
    if (candidates.empty()) {
        std::cout << "[COMBAT] No targetable NPCs nearby\n";
        return;
    }
    
    // Sort by distance
    std::sort(candidates.begin(), candidates.end(),
        [](const TargetCandidate& a, const TargetCandidate& b) {
            return a.distanceSq < b.distanceSq;
        });
    
    // Find current target in list
    int currentIndex = -1;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i].id == combat.selectedTargetId) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }
    
    // Cycle to next/previous
    int newIndex;
    if (currentIndex == -1) {
        // No current target, select nearest
        newIndex = 0;
    } else {
        if (forward) {
            newIndex = (currentIndex + 1) % static_cast<int>(candidates.size());
        } else {
            newIndex = (currentIndex - 1 + static_cast<int>(candidates.size())) % static_cast<int>(candidates.size());
        }
    }
    
    // Update selection
    combat.selectedTargetId = candidates[newIndex].id;
    
    // Log selection
    auto it = entities.find(combat.selectedTargetId);
    if (it != entities.end()) {
        std::ostringstream oss;
        oss << "Target: " << (it->second.name.empty() ? "Entity" : it->second.name)
            << " (ID " << combat.selectedTargetId << ") - "
            << std::fixed << std::setprecision(1)
            << std::sqrt(candidates[newIndex].distanceSq) << " units";
        
        combat.combatLog.push_back(oss.str());
        if (combat.combatLog.size() > combat.MAX_LOG_LINES) {
            combat.combatLog.pop_front();
        }
        
        std::cout << "[COMBAT] " << oss.str() << "\n";
    }
}

// ============================================================================
// VizCombat_DrawHoverTooltip
// ============================================================================

void VizCombat_DrawHoverTooltip(
    sf::RenderWindow& window,
    const VizWorldState& worldState,
    sf::Vector2f mouseScreenPos,
    sf::Vector2f cameraWorld,
    float pixelsPerWorldUnit,
    const sf::Font* font,
    float hoverRadiusPx)
{
    if (!font) {
        return; // Font not loaded
    }
    
    const auto& entities = worldState.getEntities();
    
    // Find nearest entity within hover radius
    std::uint64_t hoveredId = 0;
    float nearestDistSq = hoverRadiusPx * hoverRadiusPx;
    const VizEntity* hoveredEntity = nullptr;
    
    for (const auto& [id, entity] : entities) {
        // Skip local player
        if (entity.isLocalPlayer) {
            continue;
        }
        
        // Transform entity to screen space
        sf::Vector2f entityScreen = WorldToScreen(
            entity.posX, entity.posY,
            cameraWorld, pixelsPerWorldUnit);
        
        // Check distance to mouse
        float distSq = DistanceSquared(mouseScreenPos, entityScreen);
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            hoveredId = id;
            hoveredEntity = &entity;
        }
    }
    
    // Draw tooltip if hovering over entity
    if (hoveredEntity) {
        const float fontSize = 12.0f;
        const float padding = 4.0f;
        const float offsetX = 15.0f;
        const float offsetY = 15.0f;
        
        // Build tooltip text
        std::ostringstream oss;
        oss << (hoveredEntity->name.empty() ? "Entity" : hoveredEntity->name);
        if (hoveredEntity->maxHp > 0) {
            oss << " (" << hoveredEntity->hp << "/" << hoveredEntity->maxHp << ")";
        }
        
        sf::Text tooltipText(*font);
        tooltipText.setString(oss.str());
        tooltipText.setCharacterSize(static_cast<unsigned int>(fontSize));
        tooltipText.setFillColor(sf::Color::White);
        
        // Position near mouse cursor
        tooltipText.setPosition({ 
            mouseScreenPos.x + offsetX, 
            mouseScreenPos.y + offsetY 
        });
        
        // Draw background
        sf::FloatRect textBounds = tooltipText.getLocalBounds();
        sf::RectangleShape background({
            textBounds.size.x + padding * 2.0f,
            textBounds.size.y + padding * 2.0f
        });
        background.setPosition({ 
            mouseScreenPos.x + offsetX - padding, 
            mouseScreenPos.y + offsetY - padding 
        });
        background.setFillColor(sf::Color(0, 0, 0, 200));
        background.setOutlineColor(sf::Color(100, 100, 100));
        background.setOutlineThickness(1.0f);
        
        window.draw(background);
        window.draw(tooltipText);
    }
}
