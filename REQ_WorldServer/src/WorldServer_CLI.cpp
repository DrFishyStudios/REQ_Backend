#include "../include/req/world/WorldServer.h"

#include "../../REQ_Shared/include/req/shared/Logger.h"

#include <iostream>
#include <sstream>
#include <string>

namespace req::world {

void WorldServer::runCLI() {
    req::shared::logInfo("world", "");
    req::shared::logInfo("world", "=== WorldServer CLI ===");
    req::shared::logInfo("world", "Type 'help' for available commands, 'quit' to exit");
    req::shared::logInfo("world", "");
    
    std::string line;
    while (true) {
        std::cout << "\n> ";
        if (!std::getline(std::cin, line)) {
            break; // EOF or error
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);
        
        if (line.empty()) {
            continue;
        }
        
        if (line == "quit" || line == "exit" || line == "q") {
            req::shared::logInfo("world", "CLI quit requested - shutting down server");
            stop();
            break;
        }
        
        try {
            handleCLICommand(line);
        } catch (const std::exception& e) {
            req::shared::logError("world", std::string{"CLI command error: "} + e.what());
        }
    }
}

void WorldServer::handleCLICommand(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "help" || cmd == "?") {
        cmdHelp();
    } else if (cmd == "list_accounts") {
        cmdListAccounts();
    } else if (cmd == "list_chars") {
        std::uint64_t accountId = 0;
        if (!(iss >> accountId)) {
            req::shared::logError("world", "Usage: list_chars <accountId>");
            return;
        }
        cmdListChars(accountId);
    } else if (cmd == "show_char") {
        std::uint64_t characterId = 0;
        if (!(iss >> characterId)) {
            req::shared::logError("world", "Usage: show_char <characterId>");
            return;
        }
        cmdShowChar(characterId);
    } else {
        req::shared::logWarn("world", std::string{"Unknown command: '"} + cmd + "' (type 'help' for commands)");
    }
}

void WorldServer::cmdHelp() {
    std::cout << "\n=== WorldServer CLI Commands ===\n";
    std::cout << "  help, ?                  Show this help message\n";
    std::cout << "  list_accounts            List all accounts\n";
    std::cout << "  list_chars <accountId>   List all characters for an account\n";
    std::cout << "  show_char <characterId>  Show detailed character information\n";
    std::cout << "  quit, exit, q            Shutdown the server\n";
    std::cout << "===============================\n";
}

void WorldServer::cmdListAccounts() {
    auto accounts = accountStore_.loadAllAccounts();
    
    if (accounts.empty()) {
        req::shared::logInfo("world", "No accounts found");
        return;
    }
    
    req::shared::logInfo("world", std::string{"Found "} + std::to_string(accounts.size()) + " account(s):");
    
    for (const auto& account : accounts) {
        std::cout << "  id=" << account.accountId
                  << " username=" << account.username
                  << " display=\"" << account.displayName << "\""
                  << " admin=" << (account.isAdmin ? "Y" : "N")
                  << " banned=" << (account.isBanned ? "Y" : "N")
                  << "\n";
    }
}

void WorldServer::cmdListChars(std::uint64_t accountId) {
    // Verify account exists
    auto account = accountStore_.loadById(accountId);
    if (!account.has_value()) {
        req::shared::logError("world", std::string{"Account not found: id="} + std::to_string(accountId));
        return;
    }
    
    req::shared::logInfo("world", std::string{"Characters for accountId="} + std::to_string(accountId) +
        " (username=" + account->username + "):");
    
    // Load characters for all worlds (world filter removed for debugging)
    auto characters = characterStore_.loadCharactersForAccountAndWorld(accountId, config_.worldId);
    
    if (characters.empty()) {
        std::cout << "  (no characters)\n";
        return;
    }
    
    for (const auto& ch : characters) {
        std::cout << "  id=" << ch.characterId
                  << " name=" << ch.name
                  << " race=" << ch.race
                  << " class=" << ch.characterClass
                  << " lvl=" << ch.level
                  << " zone=" << ch.lastZoneId
                  << " pos=(" << ch.positionX << "," << ch.positionY << "," << ch.positionZ << ")"
                  << "\n";
    }
}

void WorldServer::cmdShowChar(std::uint64_t characterId) {
    auto character = characterStore_.loadById(characterId);
    
    if (!character.has_value()) {
        req::shared::logError("world", std::string{"Character not found: id="} + std::to_string(characterId));
        return;
    }
    
    const auto& ch = *character;
    
    std::cout << "\n=== Character Details ===\n";
    std::cout << "Character ID:     " << ch.characterId << "\n";
    std::cout << "Account ID:       " << ch.accountId << "\n";
    std::cout << "Name:             " << ch.name << "\n";
    std::cout << "Race:             " << ch.race << "\n";
    std::cout << "Class:            " << ch.characterClass << "\n";
    std::cout << "Level:            " << ch.level << "\n";
    std::cout << "XP:               " << ch.xp << "\n";
    std::cout << "\n";
    std::cout << "Home World:       " << ch.homeWorldId << "\n";
    std::cout << "Last World:       " << ch.lastWorldId << "\n";
    std::cout << "Last Zone:        " << ch.lastZoneId << "\n";
    std::cout << "\n";
    std::cout << "Position:         (" << ch.positionX << ", " << ch.positionY << ", " << ch.positionZ << ")\n";
    std::cout << "Heading:          " << ch.heading << " degrees\n";
    std::cout << "\n";
    std::cout << "Bind World:       " << ch.bindWorldId << "\n";
    std::cout << "Bind Zone:        " << ch.bindZoneId << "\n";
    std::cout << "Bind Position:    (" << ch.bindX << ", " << ch.bindY << ", " << ch.bindZ << ")\n";
    std::cout << "\n";
    std::cout << "HP:               " << ch.hp << " / " << ch.maxHp << "\n";
    std::cout << "Mana:             " << ch.mana << " / " << ch.maxMana << "\n";
    std::cout << "\n";
    std::cout << "Stats:\n";
    std::cout << "  STR: " << ch.strength << "  STA: " << ch.stamina << "\n";
    std::cout << "  AGI: " << ch.agility << "  DEX: " << ch.dexterity << "\n";
    std::cout << "  WIS: " << ch.wisdom << "  INT: " << ch.intelligence << "\n";
    std::cout << "  CHA: " << ch.charisma << "\n";
    std::cout << "=========================\n";
}

} // namespace req::world
