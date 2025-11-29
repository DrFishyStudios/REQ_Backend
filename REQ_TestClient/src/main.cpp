#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../include/req/testclient/TestClient.h"

#include <string>
#include <iostream>

void showMenu() {
    std::cout << "\n========================================\n";
    std::cout << "  REQ Backend Test Client - Test Menu\n";
    std::cout << "========================================\n";
    std::cout << "1. Happy Path Scenario (automated full handshake)\n";
    std::cout << "2. Bad Password Test\n";
    std::cout << "3. Bad Session Token Test\n";
    std::cout << "4. Bad Handoff Token Test\n";
    std::cout << "5. Negative Tests (malformed payloads)\n";
    std::cout << "6. Interactive Mode (original flow)\n";
    std::cout << "q. Quit\n";
    std::cout << "========================================\n";
    std::cout << "Select option: ";
}

int main(int argc, char* argv[]) {
    req::shared::initLogger("REQ_TestClient");
    
    req::testclient::TestClient client;
    
    // Check for command-line arguments
    if (argc > 1) {
        std::string arg = argv[1];
        
        if (arg == "--happy-path" || arg == "-h") {
            client.runHappyPathScenario();
            return 0;
        }
        else if (arg == "--bad-password" || arg == "-bp") {
            client.runBadPasswordTest();
            return 0;
        }
        else if (arg == "--bad-session" || arg == "-bs") {
            client.runBadSessionTokenTest();
            return 0;
        }
        else if (arg == "--bad-handoff" || arg == "-bh") {
            client.runBadHandoffTokenTest();
            return 0;
        }
        else if (arg == "--negative-tests" || arg == "-n") {
            client.runNegativeTests();
            return 0;
        }
        else if (arg == "--interactive" || arg == "-i") {
            client.run();
            return 0;
        }
        else if (arg == "--help") {
            std::cout << "REQ_TestClient - Backend Handshake Test Harness\n\n";
            std::cout << "Usage: REQ_TestClient.exe [option]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --happy-path, -h     Run automated happy path scenario\n";
            std::cout << "  --bad-password, -bp  Test bad password handling\n";
            std::cout << "  --bad-session, -bs   Test bad session token handling\n";
            std::cout << "  --bad-handoff, -bh   Test bad handoff token handling\n";
            std::cout << "  --negative-tests, -n Run malformed payload tests\n";
            std::cout << "  --interactive, -i    Original interactive mode\n";
            std::cout << "  --help               Show this help\n\n";
            std::cout << "If no option is provided, interactive menu will be shown.\n";
            return 0;
        }
        else {
            std::cout << "Unknown option: " << arg << "\n";
            std::cout << "Use --help for usage information.\n";
            return 1;
        }
    }
    
    // No command-line args - show interactive menu
    bool running = true;
    while (running) {
        showMenu();
        
        std::string choice;
        std::getline(std::cin, choice);
        
        // Trim whitespace
        choice.erase(0, choice.find_first_not_of(" \t\n\r"));
        choice.erase(choice.find_last_not_of(" \t\n\r") + 1);
        
        if (choice.empty()) {
            continue;
        }
        
        if (choice == "1") {
            client.runHappyPathScenario();
        }
        else if (choice == "2") {
            client.runBadPasswordTest();
        }
        else if (choice == "3") {
            client.runBadSessionTokenTest();
        }
        else if (choice == "4") {
            client.runBadHandoffTokenTest();
        }
        else if (choice == "5") {
            client.runNegativeTests();
        }
        else if (choice == "6") {
            client.run();
        }
        else if (choice == "q" || choice == "Q") {
            running = false;
            std::cout << "Exiting...\n";
        }
        else {
            std::cout << "Invalid choice. Please select 1-6 or q.\n";
        }
    }
    
    return 0;
}
