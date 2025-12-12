#pragma once

#include <cstdint>
#include <string>
#include <SFML/Graphics.hpp>
#include "VizWorldState.h"
#include "VizCombat.h"

// ============================================================================
// VizHUDData - Data passed to HUD for rendering
// ============================================================================

struct VizHUDData {
    // FPS
    float fps{ 0.0f };
    
    // Local player position
    float localPosX{ 0.0f };
    float localPosY{ 0.0f };
    float localPosZ{ 0.0f };
    bool hasLocalPlayer{ false };
    
    // Message counters
    std::uint32_t snapshotCount{ 0 };
    std::uint32_t spawnCount{ 0 };
    std::uint32_t updateCount{ 0 };
    std::uint32_t despawnCount{ 0 };
    std::uint32_t attackResultCount{ 0 };
    std::uint32_t devResponseCount{ 0 };
    
    // Target info (from combat state)
    std::uint64_t targetId{ 0 };
    std::string targetName;
    std::int32_t targetHp{ 0 };
    std::int32_t targetMaxHp{ 0 };
    bool hasTarget{ false };
};

// ============================================================================
// VizHUD API
// ============================================================================

/**
 * VizHUD_Draw
 * 
 * Draws HUD overlay with diagnostics and target info.
 * Only draws if HUD is enabled.
 * 
 * @param window SFML window
 * @param data HUD data to display
 * @param font SFML font for text (may be null if font failed to load)
 * @param enabled Whether HUD is visible
 */
void VizHUD_Draw(
    sf::RenderWindow& window,
    const VizHUDData& data,
    const sf::Font* font,
    bool enabled);
