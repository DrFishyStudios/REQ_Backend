#pragma once

#include <cstdint>
#include <string>
#include <deque>
#include <SFML/Graphics.hpp>

#include <req/clientcore/ClientCore.h>
#include "VizWorldState.h"

// ============================================================================
// VizCombatState - Manages targeting and attack state for VizTestClient
// ============================================================================

struct VizCombatState {
    // Targeting
    std::uint64_t selectedTargetId{ 0 };
    
    // Attack cooldown
    sf::Clock attackClock;
    float attackCooldownSec{ 0.25f };
    
    // Combat log (ring buffer of last N lines)
    std::deque<std::string> combatLog;
    static constexpr std::size_t MAX_LOG_LINES = 20;
    
    // Stats
    std::uint32_t attacksSent{ 0 };
    std::uint32_t attacksReceived{ 0 };
};

// ============================================================================
// VizCombat API
// ============================================================================

/**
 * VizCombat_HandleMouseClickSelect
 * 
 * Attempts to select an entity near the mouse click position.
 * Selects the nearest entity within a pixel radius.
 * 
 * @param combat Combat state to update
 * @param worldState World state containing entities
 * @param mouseScreenPos Mouse position in screen space
 * @param cameraWorld Camera position in world space
 * @param pixelsPerWorldUnit Scale factor
 * @param selectRadiusPx Selection radius in pixels (default 12.0f)
 */
void VizCombat_HandleMouseClickSelect(
    VizCombatState& combat,
    const VizWorldState& worldState,
    sf::Vector2f mouseScreenPos,
    sf::Vector2f cameraWorld,
    float pixelsPerWorldUnit,
    float selectRadiusPx = 12.0f);

/**
 * VizCombat_HandleAttackKey
 * 
 * Handles F key press to send attack request.
 * Only sends if target is selected and cooldown has elapsed.
 * 
 * @param combat Combat state
 * @param session Client session (for sendAttackRequest)
 * @return true if attack was sent, false otherwise
 */
bool VizCombat_HandleAttackKey(
    VizCombatState& combat,
    const req::clientcore::ClientSession& session);

/**
 * VizCombat_HandleAttackResult
 * 
 * Parses and logs an AttackResult message.
 * Updates combat log with damage/miss info.
 * 
 * @param combat Combat state
 * @param payload Raw payload from ZoneMessage
 * @return true if parse succeeded, false otherwise
 */
bool VizCombat_HandleAttackResult(
    VizCombatState& combat,
    const std::string& payload);

/**
 * VizCombat_DrawTargetIndicator
 * 
 * Draws a yellow ring around the selected target.
 * Optionally draws HP bar above target.
 * 
 * @param window SFML window
 * @param combat Combat state
 * @param worldState World state
 * @param cameraWorld Camera position
 * @param pixelsPerWorldUnit Scale factor
 */
void VizCombat_DrawTargetIndicator(
    sf::RenderWindow& window,
    const VizCombatState& combat,
    const VizWorldState& worldState,
    sf::Vector2f cameraWorld,
    float pixelsPerWorldUnit);

/**
 * VizCombat_ClearTargetIfDespawned
 * 
 * Clears selected target if entity no longer exists.
 * Call this after processing EntityDespawn messages.
 * 
 * @param combat Combat state
 * @param worldState World state
 */
void VizCombat_ClearTargetIfDespawned(
    VizCombatState& combat,
    const VizWorldState& worldState);

/**
 * VizCombat_CycleTarget
 * 
 * Cycles through visible NPCs by distance from local player.
 * 
 * @param combat Combat state
 * @param worldState World state
 * @param localCharacterId Local player character ID
 * @param forward true = Tab (next), false = Shift+Tab (previous)
 */
void VizCombat_CycleTarget(
    VizCombatState& combat,
    const VizWorldState& worldState,
    std::uint64_t localCharacterId,
    bool forward = true);

/**
 * VizCombat_DrawHoverTooltip
 * 
 * Draws tooltip for entity near mouse cursor.
 * Shows name + HP if within hover radius.
 * 
 * @param window SFML window
 * @param worldState World state
 * @param mouseScreenPos Mouse position in screen space
 * @param cameraWorld Camera position in world space
 * @param pixelsPerWorldUnit Scale factor
 * @param font Font for tooltip text (may be null)
 * @param hoverRadiusPx Hover detection radius in pixels (default 12.0f)
 */
void VizCombat_DrawHoverTooltip(
    sf::RenderWindow& window,
    const VizWorldState& worldState,
    sf::Vector2f mouseScreenPos,
    sf::Vector2f cameraWorld,
    float pixelsPerWorldUnit,
    const sf::Font* font,
    float hoverRadiusPx = 12.0f);
