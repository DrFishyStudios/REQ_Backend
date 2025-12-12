#include "VizHUD.h"
#include "VizUiScale.h"
#include <sstream>
#include <iomanip>

// ============================================================================
// VizHUD_Draw
// ============================================================================

void VizHUD_Draw(
    sf::RenderWindow& window,
    const VizHUDData& data,
    const sf::Font* font,
    bool enabled)
{
    if (!enabled || !font) {
        return; // HUD disabled or font not loaded
    }
    
    const auto windowSize = window.getSize();
    const float windowWidth = static_cast<float>(windowSize.x);
    const float windowHeight = static_cast<float>(windowSize.y);
    
    // Unified UI scaling - consistent with console
    const unsigned int fontSize = VizUi::GetUiFontPx(windowHeight, 24u, 48u);
    const float lineHeight = static_cast<float>(fontSize) + 6.0f;
    const float padding = 16.0f;
    
    // Helper to create text
    auto createText = [&](const std::string& str, float x, float y, sf::Color color = sf::Color::White) {
        sf::Text text(*font);
        text.setString(str);
        text.setCharacterSize(fontSize);
        text.setFillColor(color);
        text.setPosition({ x, y });
        return text;
    };
    
    // ========================================================================
    // Top-Left Panel: Local Player + FPS
    // ========================================================================
    float yPos = padding;
    
    // FPS
    {
        std::ostringstream oss;
        oss << "FPS: " << std::fixed << std::setprecision(1) << data.fps;
        auto text = createText(oss.str(), padding, yPos, sf::Color::Yellow);
        window.draw(text);
        yPos += lineHeight;
    }
    
    // Local player position
    if (data.hasLocalPlayer) {
        std::ostringstream oss;
        oss << "Pos: (" << std::fixed << std::setprecision(1)
            << data.localPosX << ", "
            << data.localPosY << ", "
            << data.localPosZ << ")";
        auto text = createText(oss.str(), padding, yPos);
        window.draw(text);
        yPos += lineHeight;
    } else {
        auto text = createText("Pos: (not found)", padding, yPos, sf::Color(150, 150, 150));
        window.draw(text);
        yPos += lineHeight;
    }
    
    // ========================================================================
    // Top-Left Panel: Message Counters
    // ========================================================================
    yPos += lineHeight * 0.5f; // Small gap
    
    {
        std::ostringstream oss;
        oss << "Messages:";
        auto text = createText(oss.str(), padding, yPos, sf::Color(200, 200, 200));
        window.draw(text);
        yPos += lineHeight;
    }
    
    {
        std::ostringstream oss;
        oss << "  Snapshots: " << data.snapshotCount;
        auto text = createText(oss.str(), padding, yPos, sf::Color(180, 180, 180));
        window.draw(text);
        yPos += lineHeight;
    }
    
    {
        std::ostringstream oss;
        oss << "  Spawns: " << data.spawnCount;
        auto text = createText(oss.str(), padding, yPos, sf::Color(180, 180, 180));
        window.draw(text);
        yPos += lineHeight;
    }
    
    {
        std::ostringstream oss;
        oss << "  Updates: " << data.updateCount;
        auto text = createText(oss.str(), padding, yPos, sf::Color(180, 180, 180));
        window.draw(text);
        yPos += lineHeight;
    }
    
    {
        std::ostringstream oss;
        oss << "  Despawns: " << data.despawnCount;
        auto text = createText(oss.str(), padding, yPos, sf::Color(180, 180, 180));
        window.draw(text);
        yPos += lineHeight;
    }
    
    {
        std::ostringstream oss;
        oss << "  Attacks: " << data.attackResultCount;
        auto text = createText(oss.str(), padding, yPos, sf::Color(180, 180, 180));
        window.draw(text);
        yPos += lineHeight;
    }
    
    {
        std::ostringstream oss;
        oss << "  DevCmds: " << data.devResponseCount;
        auto text = createText(oss.str(), padding, yPos, sf::Color(180, 180, 180));
        window.draw(text);
        yPos += lineHeight;
    }
    
    // ========================================================================
    // Top-Right Panel: Target Info
    // ========================================================================
    float rightX = windowWidth - padding;
    yPos = padding;
    
    if (data.hasTarget && data.targetId != 0) {
        // Target ID + Name
        {
            std::ostringstream oss;
            oss << "Target: " << (data.targetName.empty() ? "Entity" : data.targetName);
            auto text = createText(oss.str(), 0.0f, yPos, sf::Color::Yellow);
            // Right-align
            float textWidth = text.getLocalBounds().size.x;
            text.setPosition({ rightX - textWidth, yPos });
            window.draw(text);
            yPos += lineHeight;
        }
        
        // Target ID
        {
            std::ostringstream oss;
            oss << "ID: " << data.targetId;
            auto text = createText(oss.str(), 0.0f, yPos);
            float textWidth = text.getLocalBounds().size.x;
            text.setPosition({ rightX - textWidth, yPos });
            window.draw(text);
            yPos += lineHeight;
        }
        
        // Target HP
        if (data.targetMaxHp > 0) {
            std::ostringstream oss;
            oss << "HP: " << data.targetHp << " / " << data.targetMaxHp;
            
            // Color-code by HP percentage
            float hpPercent = static_cast<float>(data.targetHp) / static_cast<float>(data.targetMaxHp);
            sf::Color hpColor;
            if (hpPercent > 0.75f) {
                hpColor = sf::Color::Green;
            } else if (hpPercent > 0.5f) {
                hpColor = sf::Color::Yellow;
            } else if (hpPercent > 0.25f) {
                hpColor = sf::Color(255, 165, 0); // Orange
            } else {
                hpColor = sf::Color::Red;
            }
            
            auto text = createText(oss.str(), 0.0f, yPos, hpColor);
            float textWidth = text.getLocalBounds().size.x;
            text.setPosition({ rightX - textWidth, yPos });
            window.draw(text);
            yPos += lineHeight;
        }
    } else {
        auto text = createText("No Target", 0.0f, yPos, sf::Color(150, 150, 150));
        float textWidth = text.getLocalBounds().size.x;
        text.setPosition({ rightX - textWidth, yPos });
        window.draw(text);
    }
}
