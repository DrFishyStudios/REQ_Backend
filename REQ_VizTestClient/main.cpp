#include <SFML/Graphics.hpp>
#include <optional>
#include <iostream>
#include <string>
#include <cstdint>
#include <deque>
#include <cmath>
#include <chrono>

#include <req/clientcore/ClientCore.h>
#include <req/shared/MessageTypes.h>
#include "VizWorldState.h"
#include "VizCombat.h"
#include "VizConsole.h"
#include "VizHUD.h"

// ============================================================================
// DrawGrid - Helper for visual reference
// ============================================================================
void DrawGrid(sf::RenderWindow& window,
              sf::Vector2u windowSize,
              sf::Vector2f cameraWorld,
              float pixelsPerWorldUnit)
{
    // Guard against invalid scale
    if (pixelsPerWorldUnit <= 0.0f) {
        pixelsPerWorldUnit = 1.0f;
    }

    // Desired grid spacing on screen (pixels)
    const float desiredGridPx = 80.0f;
    
    // Compute grid spacing in world units
    const float gridSpacingWorld = desiredGridPx / pixelsPerWorldUnit;
    
    // Guard against invalid spacing
    if (gridSpacingWorld <= 0.0f || !std::isfinite(gridSpacingWorld)) {
        return; // Skip grid if math is broken
    }

    const float halfWidth = static_cast<float>(windowSize.x) / 2.0f;
    const float halfHeight = static_cast<float>(windowSize.y) / 2.0f;

    // Lambda to convert world coords to screen coords
    auto worldToScreen = [&](float wx, float wy) -> sf::Vector2f {
        float screenX = halfWidth + (wx - cameraWorld.x) * pixelsPerWorldUnit;
        float screenY = halfHeight - (wy - cameraWorld.y) * pixelsPerWorldUnit;
        return sf::Vector2f{ screenX, screenY };
    };

    // Compute visible world bounds
    const float visibleWorldWidth = static_cast<float>(windowSize.x) / pixelsPerWorldUnit;
    const float visibleWorldHeight = static_cast<float>(windowSize.y) / pixelsPerWorldUnit;
    
    const float minWorldX = cameraWorld.x - visibleWorldWidth * 0.5f;
    const float maxWorldX = cameraWorld.x + visibleWorldWidth * 0.5f;
    const float minWorldY = cameraWorld.y - visibleWorldHeight * 0.5f;
    const float maxWorldY = cameraWorld.y + visibleWorldHeight * 0.5f;

    // Compute grid line indices (snap to grid boundaries)
    const int firstGridX = static_cast<int>(std::floor(minWorldX / gridSpacingWorld));
    const int lastGridX = static_cast<int>(std::ceil(maxWorldX / gridSpacingWorld));
    const int firstGridY = static_cast<int>(std::floor(minWorldY / gridSpacingWorld));
    const int lastGridY = static_cast<int>(std::ceil(maxWorldY / gridSpacingWorld));

    // Safety cap (prevent excessive lines)
    const int maxLines = 1000;
    if ((lastGridX - firstGridX) > maxLines || (lastGridY - firstGridY) > maxLines) {
        return; // Too many lines, skip grid
    }

    // Draw vertical lines
    for (int ix = firstGridX; ix <= lastGridX; ++ix) {
        float wx = ix * gridSpacingWorld;
        
        // Every 10th line is major
        bool isMajor = (ix % 10 == 0);
        sf::Color lineColor = isMajor ? sf::Color(100, 100, 100, 255) : sf::Color(50, 50, 50, 255);

        sf::Vector2f topScreen = worldToScreen(wx, maxWorldY);
        sf::Vector2f botScreen = worldToScreen(wx, minWorldY);

        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0] = sf::Vertex(topScreen, lineColor);
        line[1] = sf::Vertex(botScreen, lineColor);
        window.draw(line);
    }

    // Draw horizontal lines
    for (int iy = firstGridY; iy <= lastGridY; ++iy) {
        float wy = iy * gridSpacingWorld;
        
        // Every 10th line is major
        bool isMajor = (iy % 10 == 0);
        sf::Color lineColor = isMajor ? sf::Color(100, 100, 100, 255) : sf::Color(50, 50, 50, 255);

        sf::Vector2f leftScreen = worldToScreen(minWorldX, wy);
        sf::Vector2f rightScreen = worldToScreen(maxWorldX, wy);

        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0] = sf::Vertex(leftScreen, lineColor);
        line[1] = sf::Vertex(rightScreen, lineColor);
        window.draw(line);
    }

    // Draw center crosshair (marks camera origin)
    const float crossSize = 10.0f;
    sf::Vector2f center{ halfWidth, halfHeight };
    
    // Horizontal crosshair
    sf::VertexArray crossH(sf::PrimitiveType::Lines, 2);
    crossH[0] = sf::Vertex(sf::Vector2f(center.x - crossSize, center.y), sf::Color(255, 255, 0, 200));
    crossH[1] = sf::Vertex(sf::Vector2f(center.x + crossSize, center.y), sf::Color(255, 255, 0, 200));
    window.draw(crossH);
    
    // Vertical crosshair
    sf::VertexArray crossV(sf::PrimitiveType::Lines, 2);
    crossV[0] = sf::Vertex(sf::Vector2f(center.x, center.y - crossSize), sf::Color(255, 255, 0, 200));
    crossV[1] = sf::Vertex(sf::Vector2f(center.x, center.y + crossSize), sf::Color(255, 255, 0, 200));
    window.draw(crossV);
}

int main()
{
    using namespace req::clientcore;

    // ---------------------------------------------------------------------
    // 1) Setup client config + session
    // ---------------------------------------------------------------------
    ClientConfig config{};
    ClientSession session{};

    // TODO: Replace with your actual test credentials / config
    const std::string username = "testuser";
    const std::string password = "testpass";

    std::cout << "[REQ_VizTestClient] Logging in as '" << username << "'...\n";

    auto loginResp = login(
        config,
        username,
        password,
        req::shared::protocol::LoginMode::Login,
        session
    );

    if (loginResp.result != LoginResult::Success)
    {
        std::cerr << "[REQ_VizTestClient] Login failed: "
            << loginResp.errorMessage << "\n";
        return 1;
    }

    std::cout << "[REQ_VizTestClient] Login OK. Worlds available: "
        << loginResp.availableWorlds.size() << "\n";
    
    // Store admin status for console
    const bool isAdmin = session.isAdmin;

    // ---------------------------------------------------------------------
    // 2) Character list / create / select
    // ---------------------------------------------------------------------
    std::cout << "[REQ_VizTestClient] Requesting character list...\n";
    auto charListResp = getCharacterList(session);

    if (charListResp.result != CharacterListResult::Success)
    {
        std::cerr << "[REQ_VizTestClient] Character list failed: "
            << charListResp.errorMessage << "\n";
        return 1;
    }

    std::uint64_t chosenCharacterId = 0;

    if (!charListResp.characters.empty())
    {
        const auto& ch = charListResp.characters.front();
        chosenCharacterId = ch.characterId;

        std::cout << "[REQ_VizTestClient] Using existing character: "
            << ch.name << " (id=" << ch.characterId << ")\n";
    }
    else
    {
        std::cout << "[REQ_VizTestClient] No characters found, creating one...\n";

        const std::string newName = "VizTester";
        const std::string race = "Human";
        const std::string characterClass = "Warrior";

        auto createResp = createCharacter(session, newName, race, characterClass);
        if (createResp.result != CharacterListResult::Success)
        {
            std::cerr << "[REQ_VizTestClient] Character creation failed: "
                << createResp.errorMessage << "\n";
            return 1;
        }

        chosenCharacterId = createResp.newCharacter.characterId;

        std::cout << "[REQ_VizTestClient] Created character: "
            << newName << " (id=" << chosenCharacterId << ")\n";
    }

    // ---------------------------------------------------------------------
    // 3) Enter world + connect to zone
    // ---------------------------------------------------------------------
    std::cout << "[REQ_VizTestClient] Entering world...\n";
    auto enterResp = enterWorld(session, chosenCharacterId);

    if (enterResp.result != EnterWorldResult::Success)
    {
        std::cerr << "[REQ_VizTestClient] Enter world failed: "
            << enterResp.errorMessage << "\n";
        return 1;
    }

    std::cout << "[REQ_VizTestClient] Connecting to zone...\n";
    auto zoneResp = connectToZone(session);

    if (zoneResp.result != ZoneAuthResult::Success)
    {
        std::cerr << "[REQ_VizTestClient] Zone connect failed: "
            << zoneResp.errorMessage << "\n";
        return 1;
    }

    std::cout << "[REQ_VizTestClient] Zone connection established.\n";

    // Client-side viz state
    VizWorldState worldState;
    worldState.setLocalCharacterId(chosenCharacterId);
    
    // Combat state
    VizCombatState combat;
    
    // Console state
    VizConsoleState console;
    console.isAdmin = isAdmin;
    
    // Load font for console (use system font or embed one)
    sf::Font consoleFont;
    bool fontLoaded = false;
    
    if (consoleFont.openFromFile("C:\\Windows\\Fonts\\consola.ttf")) {
        fontLoaded = true;
    } else if (consoleFont.openFromFile("C:\\Windows\\Fonts\\arial.ttf")) {
        fontLoaded = true;
        std::cout << "[REQ_VizTestClient] Using fallback font: Arial\n";
    } else {
        std::cerr << "[REQ_VizTestClient] Warning: Failed to load font, HUD/console text will not display\n";
    }

    // ---------------------------------------------------------------------
    // 4) SFML window + coordinate transform
    // ---------------------------------------------------------------------
    sf::RenderWindow window(
        sf::VideoMode({ 1280, 720u }),
        "REQ VizTestClient",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    // World-to-screen scale (DO NOT CHANGE)
    const float baseScale = 1.0f;

    std::uint32_t movementSeq = 0;

    // Trail for local player (max 200 points)
    std::deque<sf::Vector2f> playerTrail;
    
    // Mouse click state (for target selection)
    bool pendingMouseClick = false;
    sf::Vector2f pendingMousePos;
    
    // HUD state
    bool hudEnabled = true; // F1 toggle
    VizHUDData hudData;
    sf::Clock fpsClock;
    int frameCount = 0;
    
    // Message counters for HUD
    std::uint32_t msgCountSnapshot = 0;
    std::uint32_t msgCountSpawn = 0;
    std::uint32_t msgCountUpdate = 0;
    std::uint32_t msgCountDespawn = 0;
    std::uint32_t msgCountAttackResult = 0;
    std::uint32_t msgCountDevResponse = 0;

    // ---------------------------------------------------------------------
    // 5) Main loop: input, zone messages, render
    // ---------------------------------------------------------------------
    while (window.isOpen())
    {
        // --- SFML events ---
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            // Console gets first chance to handle events
            if (VizConsole_HandleEvent(console, event.value())) {
                // Check if Enter was pressed while console open
                if (console.isOpen && event->is<sf::Event::KeyPressed>()) {
                    const auto* keyPressed = event->getIf<sf::Event::KeyPressed>();
                    if (keyPressed && keyPressed->code == sf::Keyboard::Key::Enter) {
                        VizConsole_SubmitLine(console, session);
                    }
                }
                continue; // Event consumed by console
            }
            
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape)
                {
                    window.close();
                }
                
                // F key - Attack
                if (keyPressed->scancode == sf::Keyboard::Scan::F)
                {
                    VizCombat_HandleAttackKey(combat, session);
                }
                
                // F1 key - Toggle HUD
                if (keyPressed->code == sf::Keyboard::Key::F1)
                {
                    hudEnabled = !hudEnabled;
                    std::cout << "[HUD] " << (hudEnabled ? "Enabled" : "Disabled") << "\n";
                }
                
                // Tab key - Cycle targets
                if (keyPressed->code == sf::Keyboard::Key::Tab)
                {
                    bool forward = !sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) &&
                                   !sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                    VizCombat_CycleTarget(combat, worldState, chosenCharacterId, forward);
                }
            }
            
            // Mouse left click - Target selection
            if (const auto* mouseClick = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouseClick->button == sf::Mouse::Button::Left)
                {
                    pendingMouseClick = true;
                    pendingMousePos = sf::Vector2f{ 
                        static_cast<float>(mouseClick->position.x), 
                        static_cast<float>(mouseClick->position.y) 
                    };
                }
            }
        }

        // --- Keyboard -> movement intent (WASD + Space) ---
        float inputX = 0.0f;
        float inputY = 0.0f;
        bool  jump = false;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W))
            inputY += 1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S))
            inputY -= 1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A))
            inputX -= 1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D))
            inputX += 1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))
            jump = true;

        if (inputX != 0.0f || inputY != 0.0f || jump)
        {
            const float yaw = 0.0f; // TODO: hook up camera/heading later

            const bool ok = sendMovementIntent(
                session,
                inputX,
                inputY,
                yaw,
                jump,
                ++movementSeq
            );

            if (!ok)
            {
                std::cerr << "[REQ_VizTestClient] sendMovementIntent failed\n";
            }
        }

        // --- Pump zone messages into VizWorldState ---
        {
            ZoneMessage msg;
            using MT = req::shared::MessageType;

            static int unhandledLogBudget = 20;

            while (tryReceiveZoneMessage(session, msg))
            {
                switch (msg.type)
                {
                case MT::PlayerStateSnapshot:
                {
                    req::shared::protocol::PlayerStateSnapshotData snapshot;
                    if (parsePlayerStateSnapshot(msg.payload, snapshot))
                    {
                        worldState.applyPlayerStateSnapshot(snapshot);
                        msgCountSnapshot++;
                    }
                    break;
                }

                case MT::EntitySpawn:
                {
                    req::shared::protocol::EntitySpawnData spawn;
                    if (parseEntitySpawn(msg.payload, spawn))
                    {
                        worldState.applyEntitySpawn(spawn);
                        msgCountSpawn++;
                    }
                    break;
                }

                case MT::EntityUpdate:
                {
                    req::shared::protocol::EntityUpdateData update;
                    if (parseEntityUpdate(msg.payload, update))
                    {
                        worldState.applyEntityUpdate(update);
                        msgCountUpdate++;
                    }
                    break;
                }

                case MT::EntityDespawn:
                {
                    req::shared::protocol::EntityDespawnData despawn;
                    if (parseEntityDespawn(msg.payload, despawn))
                    {
                        worldState.applyEntityDespawn(despawn);
                        msgCountDespawn++;
                        // Clear target if despawned entity was selected
                        VizCombat_ClearTargetIfDespawned(combat, worldState);
                    }
                    break;
                }
                
                case MT::AttackResult:
                {
                    VizCombat_HandleAttackResult(combat, msg.payload);
                    msgCountAttackResult++;
                    break;
                }
                
                case MT::DevCommandResponse:
                {
                    VizConsole_HandleDevCommandResponse(console, msg.payload);
                    msgCountDevResponse++;
                    break;
                }

                default:
                    if (unhandledLogBudget > 0)
                    {
                        std::cout << "[REQ_VizTestClient] Unhandled zone msg type = "
                            << static_cast<int>(msg.type) << "\n";
                        --unhandledLogBudget;
                    }
                    break;
                }
            }
        }

        // --- Render world state ---
        window.clear(sf::Color(30, 30, 40));

        // Camera position (centered on local player)
        sf::Vector2f cameraWorld{ 0.f, 0.f };
        bool foundLocalPlayer = false;
        for (const auto& [id, entity] : worldState.getEntities())
        {
            if (id == chosenCharacterId)
            {
                cameraWorld.x = entity.posX;
                cameraWorld.y = entity.posY;
                foundLocalPlayer = true;
                
                // Update HUD with local player position
                hudData.localPosX = entity.posX;
                hudData.localPosY = entity.posY;
                hudData.localPosZ = entity.posZ;
                hudData.hasLocalPlayer = true;
                break;
            }
        }

        // Update player trail (world space)
        if (foundLocalPlayer)
        {
            playerTrail.push_back(cameraWorld);
            if (playerTrail.size() > 200)
            {
                playerTrail.pop_front();
            }
        }

        // Camera-relative world-to-screen transform
        const float windowWidth = 1280.f;
        const float windowHeight = 720.f;
        const float pixelsPerWorldUnit = baseScale;

        auto worldToScreen = [&](float wx, float wy) -> sf::Vector2f {
            const float screenX = (windowWidth / 2.f) + (wx - cameraWorld.x) * pixelsPerWorldUnit;
            const float screenY = (windowHeight / 2.f) - (wy - cameraWorld.y) * pixelsPerWorldUnit;
            return sf::Vector2f{ screenX, screenY };
        };
        
        // Handle pending mouse click for target selection
        if (pendingMouseClick) {
            VizCombat_HandleMouseClickSelect(
                combat, worldState, pendingMousePos, 
                cameraWorld, pixelsPerWorldUnit);
            pendingMouseClick = false;
        }

        // Throttled debug logging (once every 2 seconds)
        {
            static auto lastDebugTime = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastDebugTime);
            
            if (elapsed.count() >= 2) {
                const float gridSpacingWorld = 80.0f / pixelsPerWorldUnit;
                std::cout << "[DEBUG] cameraWorld=(" << cameraWorld.x << ", " << cameraWorld.y 
                         << "), pixelsPerWorldUnit=" << pixelsPerWorldUnit 
                         << ", gridSpacingWorld=" << gridSpacingWorld << "\n";
                lastDebugTime = now;
            }
        }

        // Draw grid (before trail and entities)
        DrawGrid(window, window.getSize(), cameraWorld, pixelsPerWorldUnit);
        
        // Draw target indicator (after grid, before trail)
        VizCombat_DrawTargetIndicator(window, combat, worldState, cameraWorld, pixelsPerWorldUnit);
        
        // Draw hover tooltip (before trail, after target indicator)
        sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f mousePos{ 
            static_cast<float>(mousePixelPos.x), 
            static_cast<float>(mousePixelPos.y) 
        };
        VizCombat_DrawHoverTooltip(window, worldState, mousePos, cameraWorld, pixelsPerWorldUnit, 
                                    fontLoaded ? &consoleFont : nullptr);

        // Draw player trail
        if (playerTrail.size() > 1)
        {
            sf::VertexArray trail(sf::PrimitiveType::LineStrip, playerTrail.size());
            for (std::size_t i = 0; i < playerTrail.size(); ++i)
            {
                sf::Vector2f screenPos = worldToScreen(playerTrail[i].x, playerTrail[i].y);
                trail[i].position = screenPos;
                
                // Fade trail from transparent (oldest) to semi-opaque (newest)
                float alpha = static_cast<float>(i) / static_cast<float>(playerTrail.size() - 1);
                std::uint8_t alphaValue = static_cast<std::uint8_t>(alpha * 150);
                trail[i].color = sf::Color(0, 255, 0, alphaValue);
            }
            window.draw(trail);
        }

        // Draw entities from worldState
        for (const auto& [id, entity] : worldState.getEntities())
        {
            const sf::Vector2f screenPos = worldToScreen(entity.posX, entity.posY);

            sf::CircleShape shape;
            shape.setRadius(entity.isLocalPlayer ? 8.f : 6.f);
            shape.setOrigin({ shape.getRadius(), shape.getRadius() });

            if (entity.isNpc)
                shape.setFillColor(sf::Color::Red);
            else if (entity.isLocalPlayer)
                shape.setFillColor(sf::Color::Green);
            else
                shape.setFillColor(sf::Color::Blue);

            shape.setPosition(screenPos);
            window.draw(shape);
        }
        
        // Update HUD data (target info + message counts + FPS)
        hudData.snapshotCount = msgCountSnapshot;
        hudData.spawnCount = msgCountSpawn;
        hudData.updateCount = msgCountUpdate;
        hudData.despawnCount = msgCountDespawn;
        hudData.attackResultCount = msgCountAttackResult;
        hudData.devResponseCount = msgCountDevResponse;
        
        // Target info
        if (combat.selectedTargetId != 0) {
            const auto& entities = worldState.getEntities();
            auto it = entities.find(combat.selectedTargetId);
            if (it != entities.end()) {
                hudData.hasTarget = true;
                hudData.targetId = combat.selectedTargetId;
                hudData.targetName = it->second.name;
                hudData.targetHp = it->second.hp;
                hudData.targetMaxHp = it->second.maxHp;
            } else {
                hudData.hasTarget = false;
            }
        } else {
            hudData.hasTarget = false;
        }
        
        // FPS calculation
        frameCount++;
        if (fpsClock.getElapsedTime().asSeconds() >= 0.5f) {
            hudData.fps = frameCount / fpsClock.getElapsedTime().asSeconds();
            frameCount = 0;
            fpsClock.restart();
        }
        
        // Draw HUD
        VizHUD_Draw(window, hudData, fontLoaded ? &consoleFont : nullptr, hudEnabled);
        
        // Draw console (last, on top of everything)
        VizConsole_Draw(window, console, consoleFont);

        window.display();
    }

    // ---------------------------------------------------------------------
    // 6) Clean shutdown
    // ---------------------------------------------------------------------
    disconnectFromZone(session);

    std::cout << "[REQ_VizTestClient] Shutdown.\n";
    return 0;
}
