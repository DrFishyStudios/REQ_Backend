#pragma once

#include <string>
#include <deque>
#include <SFML/Graphics.hpp>
#include <req/clientcore/ClientCore.h>

// ============================================================================
// VizConsoleState - In-window dev/admin console for VizTestClient
// ============================================================================

struct VizConsoleState {
    // Console visibility
    bool isOpen{ false };
    
    // Input line
    std::string inputBuffer;
    std::size_t cursorPos{ 0 };
    
    // Command history (up/down arrow navigation)
    std::deque<std::string> commandHistory;
    int historyIndex{ -1 }; // -1 = not navigating, 0+ = index in history
    static constexpr std::size_t MAX_HISTORY = 50;
    
    // Output log (command responses + errors)
    std::deque<std::string> outputLog;
    static constexpr std::size_t MAX_OUTPUT_LINES = 50;
    
    // Admin check
    bool isAdmin{ false };
    
    // Blink cursor animation
    sf::Clock cursorBlinkClock;
};

// ============================================================================
// VizConsole API
// ============================================================================

/**
 * VizConsole_HandleEvent
 * 
 * Handles SFML events for console input.
 * Returns true if event was consumed by console, false if it should propagate.
 * 
 * @param console Console state
 * @param event SFML event
 * @return true if event consumed (e.g., console is open and event handled)
 */
bool VizConsole_HandleEvent(VizConsoleState& console, const sf::Event& event);

/**
 * VizConsole_SubmitLine
 * 
 * Submits the current input line as a dev command.
 * Parses command and parameters, checks admin status, sends DevCommand.
 * 
 * @param console Console state
 * @param session Client session (for sendDevCommand)
 */
void VizConsole_SubmitLine(VizConsoleState& console, const req::clientcore::ClientSession& session);

/**
 * VizConsole_HandleDevCommandResponse
 * 
 * Handles DevCommandResponse message from server.
 * Parses and logs response to console output.
 * 
 * @param console Console state
 * @param payload Raw payload from ZoneMessage
 * @return true if parse succeeded, false otherwise
 */
bool VizConsole_HandleDevCommandResponse(VizConsoleState& console, const std::string& payload);

/**
 * VizConsole_Draw
 * 
 * Draws console overlay (input bar + recent log lines).
 * Only draws if console is open.
 * 
 * @param window SFML window
 * @param console Console state
 * @param font SFML font for text rendering
 */
void VizConsole_Draw(sf::RenderWindow& window, const VizConsoleState& console, const sf::Font& font);

/**
 * VizConsole_AddLogLine
 * 
 * Adds a line to the console output log.
 * Helper for adding status messages.
 * 
 * @param console Console state
 * @param line Line to add
 */
void VizConsole_AddLogLine(VizConsoleState& console, const std::string& line);
