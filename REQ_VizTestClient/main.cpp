#include <SFML/Graphics.hpp>
#include <optional>
#include <iostream>
#include <string>
#include <cstdint>

#include <req/clientcore/ClientCore.h>
#include <req/shared/MessageTypes.h>
#include "VizWorldState.h"

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

    // ---------------------------------------------------------------------
    // 4) SFML window + coordinate transform
    // ---------------------------------------------------------------------
    sf::RenderWindow window(
        sf::VideoMode({ 1280u, 720u }),
        "REQ VizTestClient",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    // Simple world->screen transform for a top-down view
    auto worldToScreen = [](float x, float y) -> sf::Vector2f {
        const float scale = 0.000001f;   // tweak as needed
        const float centerX = 640.f;  // screen center
        const float centerY = 360.f;
        return sf::Vector2f{ centerX + x * scale, centerY - y * scale };
        };

    std::uint32_t movementSeq = 0;

    // ---------------------------------------------------------------------
    // 5) Main loop: input, zone messages, render
    // ---------------------------------------------------------------------
    while (window.isOpen())
    {
        // --- SFML events ---
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
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

            // Throttle for unhandled message logging
            static int unhandledLogBudget = 20; // only log first 20 unknowns

            while (tryReceiveZoneMessage(session, msg))
            {
                switch (msg.type)
                {
                case MT::PlayerStateSnapshot:
                    // parse + apply (we’ll wire this properly in a moment)
                    break;

                case MT::EntitySpawn:
                    // parse + apply
                    break;

                case MT::EntityUpdate:
                    // parse + apply
                    break;

                case MT::EntityDespawn:
                    // parse + apply
                    break;

                default:
                    if (unhandledLogBudget > 0)
                    {
                        std::cout << "[REQ_VizTestClient] Unhandled zone msg type = "
                            << static_cast<int>(msg.type) << "\n";
                        --unhandledLogBudget;
                    }
                    break;
                }

                if (msg.type == MT::PlayerStateSnapshot)
                {
                    req::shared::protocol::PlayerStateSnapshotData snapshot;
                    if (req::shared::protocol::parsePlayerStateSnapshotPayload(
                        msg.payload, snapshot))
                    {
                        worldState.applyPlayerStateSnapshot(snapshot);
                    }
                }
                else if (msg.type == MT::EntitySpawn)
                {
                    req::shared::protocol::EntitySpawnData spawn;
                    if (req::shared::protocol::parseEntitySpawnPayload(
                        msg.payload, spawn))
                    {
                        worldState.applyEntitySpawn(spawn);
                    }
                }
                else if (msg.type == MT::EntityUpdate)
                {
                    req::shared::protocol::EntityUpdateData update;
                    if (req::shared::protocol::parseEntityUpdatePayload(
                        msg.payload, update))
                    {
                        worldState.applyEntityUpdate(update);
                    }
                }
                else if (msg.type == MT::EntityDespawn)
                {
                    req::shared::protocol::EntityDespawnData despawn;
                    if (req::shared::protocol::parseEntityDespawnPayload(
                        msg.payload, despawn))
                    {
                        worldState.applyEntityDespawn(despawn);
                    }
                }
                // Other message types (AttackResult, DevCommandResponse, etc.)
                // can be handled later as needed.
            }
        }

        // --- Render world state ---
        window.clear(sf::Color(30, 30, 40));

        for (const auto& [id, entity] : worldState.getEntities())
        {
            const sf::Vector2f screenPos = worldToScreen(entity.posX, entity.posY);

            sf::CircleShape shape;
            shape.setRadius(entity.isLocalPlayer ? 8.f : 6.f);
            shape.setOrigin(sf::Vector2f{ shape.getRadius(), shape.getRadius() });


            if (entity.isNpc)
            {
                shape.setFillColor(sf::Color::Red);   // NPCs
            }
            else if (entity.isLocalPlayer)
            {
                shape.setFillColor(sf::Color::Green); // you
            }
            else
            {
                shape.setFillColor(sf::Color::Blue);  // other players
            }

            shape.setPosition(screenPos);
            window.draw(shape);
        }

        // --- Render world state ---
        window.clear(sf::Color(30, 30, 40));

        // Debug: always draw a test circle so we know rendering works
       /* {
            sf::CircleShape debugShape;
            debugShape.setRadius(10.f);
            debugShape.setOrigin({ debugShape.getRadius(), debugShape.getRadius() });
            debugShape.setFillColor(sf::Color::Yellow);
            debugShape.setPosition({ 100.f, 100.f });
            window.draw(debugShape);
        }*/

        // Now draw entities from worldState
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


        window.display();
    }

    // ---------------------------------------------------------------------
    // 6) Clean shutdown
    // ---------------------------------------------------------------------
    disconnectFromZone(session);

    std::cout << "[REQ_VizTestClient] Shutdown.\n";
    return 0;
}
