#include "VizConsole.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// ============================================================================
// Helper: Parse command line into tokens
// ============================================================================

namespace {
    struct ParsedCommand {
        std::string command;
        std::string param1;
        std::string param2;
    };
    
    ParsedCommand ParseCommandLine(const std::string& line) {
        ParsedCommand result;
        std::istringstream iss(line);
        
        // Parse space-delimited tokens
        iss >> result.command;
        iss >> result.param1;
        iss >> result.param2;
        
        return result;
    }
}

// ============================================================================
// VizConsole_HandleEvent
// ============================================================================

bool VizConsole_HandleEvent(VizConsoleState& console, const sf::Event& event) {
    // Toggle console with Tilde
    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
        if (keyPressed->code == sf::Keyboard::Key::Grave) { // Tilde/Grave key
            console.isOpen = !console.isOpen;
            if (console.isOpen) {
                console.inputBuffer.clear();
                console.cursorPos = 0;
                console.historyIndex = -1;
            }
            return true; // Consume event
        }
    }
    
    // If console not open, don't consume events
    if (!console.isOpen) {
        return false;
    }
    
    // Handle text input
    if (const auto* textEntered = event.getIf<sf::Event::TextEntered>()) {
        char c = static_cast<char>(textEntered->unicode);
        
        // Ignore control characters except Enter and Backspace
        if (c >= 32 && c < 127) { // Printable ASCII
            console.inputBuffer.insert(console.cursorPos, 1, c);
            console.cursorPos++;
            return true;
        }
    }
    
    // Handle key presses
    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
        // Enter - Submit command
        if (keyPressed->code == sf::Keyboard::Key::Enter) {
            // Don't submit immediately - VizConsole_SubmitLine called externally
            return true; // Consume event
        }
        
        // Backspace - Delete character
        if (keyPressed->code == sf::Keyboard::Key::Backspace) {
            if (console.cursorPos > 0) {
                console.inputBuffer.erase(console.cursorPos - 1, 1);
                console.cursorPos--;
            }
            return true;
        }
        
        // Delete - Delete character at cursor
        if (keyPressed->code == sf::Keyboard::Key::Delete) {
            if (console.cursorPos < console.inputBuffer.size()) {
                console.inputBuffer.erase(console.cursorPos, 1);
            }
            return true;
        }
        
        // Left arrow - Move cursor left
        if (keyPressed->code == sf::Keyboard::Key::Left) {
            if (console.cursorPos > 0) {
                console.cursorPos--;
            }
            return true;
        }
        
        // Right arrow - Move cursor right
        if (keyPressed->code == sf::Keyboard::Key::Right) {
            if (console.cursorPos < console.inputBuffer.size()) {
                console.cursorPos++;
            }
            return true;
        }
        
        // Up arrow - Previous command in history
        if (keyPressed->code == sf::Keyboard::Key::Up) {
            if (!console.commandHistory.empty()) {
                if (console.historyIndex == -1) {
                    console.historyIndex = static_cast<int>(console.commandHistory.size()) - 1;
                } else if (console.historyIndex > 0) {
                    console.historyIndex--;
                }
                
                if (console.historyIndex >= 0 && 
                    console.historyIndex < static_cast<int>(console.commandHistory.size())) {
                    console.inputBuffer = console.commandHistory[console.historyIndex];
                    console.cursorPos = console.inputBuffer.size();
                }
            }
            return true;
        }
        
        // Down arrow - Next command in history
        if (keyPressed->code == sf::Keyboard::Key::Down) {
            if (console.historyIndex != -1) {
                console.historyIndex++;
                
                if (console.historyIndex >= static_cast<int>(console.commandHistory.size())) {
                    console.historyIndex = -1;
                    console.inputBuffer.clear();
                    console.cursorPos = 0;
                } else {
                    console.inputBuffer = console.commandHistory[console.historyIndex];
                    console.cursorPos = console.inputBuffer.size();
                }
            }
            return true;
        }
        
        // Escape - Close console
        if (keyPressed->code == sf::Keyboard::Key::Escape) {
            console.isOpen = false;
            return true;
        }
    }
    
    return true; // Console is open, consume all events
}

// ============================================================================
// VizConsole_SubmitLine
// ============================================================================

void VizConsole_SubmitLine(VizConsoleState& console, const req::clientcore::ClientSession& session) {
    // Trim whitespace
    std::string line = console.inputBuffer;
    line.erase(0, line.find_first_not_of(" \t\n\r"));
    line.erase(line.find_last_not_of(" \t\n\r") + 1);
    
    // Ignore empty lines
    if (line.empty()) {
        console.inputBuffer.clear();
        console.cursorPos = 0;
        return;
    }
    
    // Add to history
    console.commandHistory.push_back(line);
    if (console.commandHistory.size() > console.MAX_HISTORY) {
        console.commandHistory.pop_front();
    }
    console.historyIndex = -1;
    
    // Echo command to log
    VizConsole_AddLogLine(console, "> " + line);
    
    // Parse command
    ParsedCommand parsed = ParseCommandLine(line);
    
    // Check if command is empty
    if (parsed.command.empty()) {
        console.inputBuffer.clear();
        console.cursorPos = 0;
        return;
    }
    
    // Check admin status
    if (!console.isAdmin) {
        VizConsole_AddLogLine(console, "ERROR: Not admin (dev commands require admin account)");
        std::cout << "[CONSOLE] Command rejected: not admin\n";
        console.inputBuffer.clear();
        console.cursorPos = 0;
        return;
    }
    
    // List of supported commands (from TestClient)
    const std::string supportedCommands[] = {
        "suicide", "givexp", "setlevel", "damage_self", 
        "respawn", "respawnall", "debug_hate"
    };
    
    bool isSupported = false;
    for (const auto& cmd : supportedCommands) {
        if (parsed.command == cmd) {
            isSupported = true;
            break;
        }
    }
    
    if (!isSupported) {
        VizConsole_AddLogLine(console, "Unknown command: " + parsed.command);
        VizConsole_AddLogLine(console, "Supported: suicide, givexp <amt>, setlevel <lvl>, damage_self <amt>, respawn, respawnall, debug_hate <npcId>");
        console.inputBuffer.clear();
        console.cursorPos = 0;
        return;
    }
    
    // Send DevCommand
    const bool sent = req::clientcore::sendDevCommand(
        session,
        parsed.command,
        parsed.param1,
        parsed.param2
    );
    
    if (sent) {
        std::cout << "[CONSOLE] Sent DevCommand: " << parsed.command;
        if (!parsed.param1.empty()) {
            std::cout << " " << parsed.param1;
        }
        if (!parsed.param2.empty()) {
            std::cout << " " << parsed.param2;
        }
        std::cout << "\n";
    } else {
        VizConsole_AddLogLine(console, "ERROR: Failed to send command");
        std::cerr << "[CONSOLE] Failed to send DevCommand\n";
    }
    
    // Clear input
    console.inputBuffer.clear();
    console.cursorPos = 0;
}

// ============================================================================
// VizConsole_HandleDevCommandResponse
// ============================================================================

bool VizConsole_HandleDevCommandResponse(VizConsoleState& console, const std::string& payload) {
    req::shared::protocol::DevCommandResponseData response;
    if (!req::clientcore::parseDevCommandResponse(payload, response)) {
        std::cerr << "[CONSOLE] Failed to parse DevCommandResponse\n";
        return false;
    }
    
    // Log response
    std::string logLine;
    if (response.success) {
        logLine = "[OK] " + response.message;
    } else {
        logLine = "[ERROR] " + response.message;
    }
    
    VizConsole_AddLogLine(console, logLine);
    std::cout << "[CONSOLE] " << logLine << "\n";
    
    return true;
}

// ============================================================================
// VizConsole_Draw
// ============================================================================

void VizConsole_Draw(sf::RenderWindow& window, const VizConsoleState& console, const sf::Font& font) {
    if (!console.isOpen) {
        return; // Don't draw if closed
    }
    
    const float windowWidth = static_cast<float>(window.getSize().x);
    const float windowHeight = static_cast<float>(window.getSize().y);
    
    // Console dimensions
    const float consoleHeight = 250.0f;
    const float consoleY = windowHeight - consoleHeight;
    const float padding = 10.0f;
    const float fontSize = 14.0f;
    const float lineHeight = fontSize + 4.0f;
    
    // Draw semi-transparent background
    sf::RectangleShape background({ windowWidth, consoleHeight });
    background.setPosition({ 0.0f, consoleY });
    background.setFillColor(sf::Color(0, 0, 0, 220));
    window.draw(background);
    
    // Draw border
    sf::RectangleShape border({ windowWidth, 2.0f });
    border.setPosition({ 0.0f, consoleY });
    border.setFillColor(sf::Color(100, 100, 100, 255));
    window.draw(border);
    
    // Draw output log (recent lines, bottom to top)
    const std::size_t maxVisibleLines = static_cast<std::size_t>((consoleHeight - 40.0f) / lineHeight);
    const std::size_t startIdx = console.outputLog.size() > maxVisibleLines 
        ? console.outputLog.size() - maxVisibleLines 
        : 0;
    
    float textY = consoleY + padding;
    for (std::size_t i = startIdx; i < console.outputLog.size(); ++i) {
        sf::Text text(font);
        text.setString(console.outputLog[i]);
        text.setCharacterSize(static_cast<unsigned int>(fontSize));
        text.setFillColor(sf::Color(200, 200, 200, 255));
        text.setPosition({ padding, textY });
        window.draw(text);
        textY += lineHeight;
    }
    
    // Draw input line at bottom
    const float inputY = windowHeight - 30.0f;
    
    // Prompt
    sf::Text prompt(font);
    prompt.setString("> ");
    prompt.setCharacterSize(static_cast<unsigned int>(fontSize));
    prompt.setFillColor(sf::Color(255, 255, 0, 255)); // Yellow prompt
    prompt.setPosition({ padding, inputY });
    window.draw(prompt);
    
    // Input text
    sf::Text inputText(font);
    inputText.setString(console.inputBuffer);
    inputText.setCharacterSize(static_cast<unsigned int>(fontSize));
    inputText.setFillColor(sf::Color::White);
    inputText.setPosition({ padding + 20.0f, inputY });
    window.draw(inputText);
    
    // Draw cursor (blinking)
    if (console.cursorBlinkClock.getElapsedTime().asSeconds() < 0.5f ||
        (console.cursorBlinkClock.getElapsedTime().asSeconds() >= 1.0f && 
         console.cursorBlinkClock.getElapsedTime().asSeconds() < 1.5f)) {
        
        // Calculate cursor position
        std::string beforeCursor = console.inputBuffer.substr(0, console.cursorPos);
        sf::Text cursorMeasure(font);
        cursorMeasure.setString(beforeCursor);
        cursorMeasure.setCharacterSize(static_cast<unsigned int>(fontSize));
        
        float cursorX = padding + 20.0f + cursorMeasure.getLocalBounds().size.x;
        
        sf::RectangleShape cursor({ 2.0f, fontSize });
        cursor.setPosition({ cursorX, inputY });
        cursor.setFillColor(sf::Color::White);
        window.draw(cursor);
    }
    
    // Reset blink timer if over 2 seconds
    if (console.cursorBlinkClock.getElapsedTime().asSeconds() >= 2.0f) {
        const_cast<VizConsoleState&>(console).cursorBlinkClock.restart();
    }
}

// ============================================================================
// VizConsole_AddLogLine
// ============================================================================

void VizConsole_AddLogLine(VizConsoleState& console, const std::string& line) {
    console.outputLog.push_back(line);
    if (console.outputLog.size() > console.MAX_OUTPUT_LINES) {
        console.outputLog.pop_front();
    }
}
