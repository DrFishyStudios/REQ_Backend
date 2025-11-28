/**
 * CreateTestAccounts.cpp
 * 
 * Simple utility to create test account JSON files using AccountStore.
 * This ensures the JSON format and password hashing match what LoginServer expects.
 * 
 * Usage:
 *   From x64/Debug or x64/Release directory:
 *   REQ_LoginServer.exe --create-test-accounts
 * 
 * This will create several test accounts in data/accounts/ directory.
 */

#include <iostream>
#include <vector>
#include <string>

#include "../../REQ_Shared/include/req/shared/Logger.h"
#include "../../REQ_Shared/include/req/shared/AccountStore.h"

namespace req::login {

struct TestAccountDef {
    std::string username;
    std::string password;
    bool isAdmin;
    std::string displayName;
    std::string email;
};

void createTestAccounts() {
    req::shared::logInfo("CreateTestAccounts", "=== Creating Test Accounts ===");
    
    // Define test accounts to create
    std::vector<TestAccountDef> testAccounts = {
        // Standard test account
        {
            "testuser",
            "testpass",
            false,
            "Test User",
            "test@example.com"
        },
        // EverQuest reference account (Brad McQuaid's character)
        {
            "Aradune",
            "TestPassword123!",
            false,
            "Aradune Mithara",
            "aradune@example.com"
        },
        // Admin account
        {
            "admin",
            "AdminPass123!",
            true,
            "Administrator",
            "admin@example.com"
        },
        // Another test account
        {
            "player1",
            "password123",
            false,
            "Player One",
            ""
        }
    };
    
    // Initialize AccountStore
    const std::string accountsPath = "data/accounts";
    req::shared::logInfo("CreateTestAccounts", std::string{"Using accounts path: "} + accountsPath);
    
    try {
        req::shared::AccountStore accountStore(accountsPath);
        
        int successCount = 0;
        int skipCount = 0;
        
        for (const auto& testAccount : testAccounts) {
            req::shared::logInfo("CreateTestAccounts", std::string{"Processing account: "} + testAccount.username);
            
            // Check if account already exists
            auto existing = accountStore.findByUsername(testAccount.username);
            if (existing.has_value()) {
                req::shared::logWarn("CreateTestAccounts", std::string{"  Account '"} + testAccount.username + 
                    "' already exists (ID: " + std::to_string(existing->accountId) + ") - skipping");
                skipCount++;
                continue;
            }
            
            // Create the account
            try {
                auto account = accountStore.createAccount(testAccount.username, testAccount.password);
                
                // Update optional fields if needed
                if (!testAccount.displayName.empty() && testAccount.displayName != testAccount.username) {
                    account.displayName = testAccount.displayName;
                }
                if (!testAccount.email.empty()) {
                    account.email = testAccount.email;
                }
                if (testAccount.isAdmin) {
                    account.isAdmin = true;
                }
                
                // Save updated account
                accountStore.saveAccount(account);
                
                req::shared::logInfo("CreateTestAccounts", std::string{"  ? Created account '"} + testAccount.username + 
                    "' (ID: " + std::to_string(account.accountId) + ")");
                req::shared::logInfo("CreateTestAccounts", std::string{"    Password: "} + testAccount.password);
                req::shared::logInfo("CreateTestAccounts", std::string{"    Display Name: "} + account.displayName);
                if (account.isAdmin) {
                    req::shared::logInfo("CreateTestAccounts", "    Admin: YES");
                }
                if (!account.email.empty()) {
                    req::shared::logInfo("CreateTestAccounts", std::string{"    Email: "} + account.email);
                }
                
                successCount++;
            } catch (const std::exception& e) {
                req::shared::logError("CreateTestAccounts", std::string{"  Failed to create account '"} + 
                    testAccount.username + "': " + e.what());
            }
        }
        
        req::shared::logInfo("CreateTestAccounts", "");
        req::shared::logInfo("CreateTestAccounts", "=== Summary ===");
        req::shared::logInfo("CreateTestAccounts", std::string{"  Created: "} + std::to_string(successCount));
        req::shared::logInfo("CreateTestAccounts", std::string{"  Skipped (already exist): "} + std::to_string(skipCount));
        req::shared::logInfo("CreateTestAccounts", "");
        req::shared::logInfo("CreateTestAccounts", "Test accounts are ready! You can now login with:");
        req::shared::logInfo("CreateTestAccounts", "  Username: testuser   | Password: testpass");
        req::shared::logInfo("CreateTestAccounts", "  Username: Aradune    | Password: TestPassword123!");
        req::shared::logInfo("CreateTestAccounts", "  Username: admin      | Password: AdminPass123! (Admin account)");
        req::shared::logInfo("CreateTestAccounts", "  Username: player1    | Password: password123");
        
    } catch (const std::exception& e) {
        req::shared::logError("CreateTestAccounts", std::string{"Fatal error: "} + e.what());
        throw;
    }
}

} // namespace req::login
