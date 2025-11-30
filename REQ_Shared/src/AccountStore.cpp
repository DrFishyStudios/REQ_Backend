#include "../include/req/shared/AccountStore.h"
#include "../include/req/shared/Logger.h"
#include "../include/req/shared/DataModels.h"
#include "../../REQ_Shared/thirdparty/nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace req::shared {

namespace {
    // Placeholder password hashing - NOT CRYPTOGRAPHICALLY SECURE!
    // Production MUST replace this with bcrypt, scrypt, or Argon2.
    std::string placeholderHashPassword(const std::string& plaintext) {
        // Simple std::hash for demonstration only
        std::hash<std::string> hasher;
        std::size_t hashValue = hasher(plaintext + "_salt_placeholder");
        
        std::ostringstream oss;
        oss << "PLACEHOLDER_HASH_" << hashValue;
        return oss.str();
    }
}

AccountStore::AccountStore(const std::string& accountsRootDirectory)
    : m_accountsRootDirectory(accountsRootDirectory) {
    
    // Ensure the directory exists
    try {
        if (!fs::exists(m_accountsRootDirectory)) {
            fs::create_directories(m_accountsRootDirectory);
            logInfo("AccountStore", "Created accounts directory: " + m_accountsRootDirectory);
        }
    } catch (const fs::filesystem_error& e) {
        std::string msg = "Failed to create accounts directory: " + std::string(e.what());
        logError("AccountStore", msg);
        throw std::runtime_error(msg);
    }
}

std::optional<data::Account> AccountStore::findByUsername(const std::string& username) const {
    try {
        // Naive linear scan of all account files
        // TODO: Optimize with an index (e.g., username -> account_id map)
        for (const auto& entry : fs::directory_iterator(m_accountsRootDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                auto account = loadById(std::stoull(entry.path().stem().string()));
                if (account && account->username == username) {
                    return account;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("AccountStore", "Filesystem error during findByUsername: " + std::string(e.what()));
    } catch (const std::exception& e) {
        logError("AccountStore", "Error during findByUsername: " + std::string(e.what()));
    }
    
    return std::nullopt;
}

std::optional<data::Account> AccountStore::loadById(std::uint64_t accountId) const {
    std::string filename = m_accountsRootDirectory + "/" + std::to_string(accountId) + ".json";
    
    if (!fs::exists(filename)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            logError("AccountStore", "Failed to open account file: " + filename);
            return std::nullopt;
        }
        
        json j;
        file >> j;
        
        data::Account account = j.get<data::Account>();
        
        return account;
    } catch (const json::exception& e) {
        logError("AccountStore", "JSON parse error loading account " + std::to_string(accountId) + ": " + e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        logError("AccountStore", "Error loading account " + std::to_string(accountId) + ": " + e.what());
        return std::nullopt;
    }
}

std::vector<data::Account> AccountStore::loadAllAccounts() const {
    std::vector<data::Account> accounts;
    
    try {
        for (const auto& entry : fs::directory_iterator(m_accountsRootDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                try {
                    std::uint64_t accountId = std::stoull(entry.path().stem().string());
                    auto account = loadById(accountId);
                    if (account.has_value()) {
                        accounts.push_back(*account);
                    }
                } catch (const std::exception& e) {
                    // Skip files with non-numeric names or parse errors
                    logWarn("AccountStore", "Skipping invalid account file: " + entry.path().string());
                    continue;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("AccountStore", "Filesystem error during loadAllAccounts: " + std::string(e.what()));
    }
    
    return accounts;
}

data::Account AccountStore::createAccount(const std::string& username, const std::string& passwordPlaintext) {
    // Check if username already exists
    if (findByUsername(username).has_value()) {
        std::string msg = "Account creation failed: username '" + username + "' already exists";
        logWarn("AccountStore", msg);
        throw std::runtime_error(msg);
    }
    
    // Generate new account ID
    std::uint64_t newAccountId = generateNewAccountId();
    
    // Create new account
    data::Account account;
    account.accountId = newAccountId;
    account.username = username;
    account.passwordHash = hashPassword(passwordPlaintext);
    account.isBanned = false;
    account.isAdmin = false;
    account.displayName = username; // Default to username
    account.email = "";
    
    // Save to disk
    if (!saveAccount(account)) {
        std::string msg = "Failed to save newly created account: " + username;
        logError("AccountStore", msg);
        throw std::runtime_error(msg);
    }
    
    logInfo("AccountStore", "Created new account: id=" + std::to_string(account.accountId) + 
            ", username=" + account.username);
    
    return account;
}

bool AccountStore::saveAccount(const data::Account& account) const {
    std::string filename = m_accountsRootDirectory + "/" + std::to_string(account.accountId) + ".json";
    
    try {
        json j = account;
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            logError("AccountStore", "Failed to open account file for writing: " + filename);
            return false;
        }
        
        file << j.dump(4); // Pretty print with 4-space indent
        file.close();
        
        return true;
    } catch (const json::exception& e) {
        logError("AccountStore", "JSON serialization error saving account " + 
                std::to_string(account.accountId) + ": " + e.what());
        return false;
    } catch (const std::exception& e) {
        logError("AccountStore", "Error saving account " + std::to_string(account.accountId) + ": " + e.what());
        return false;
    }
}

std::uint64_t AccountStore::generateNewAccountId() const {
    std::uint64_t maxId = 0;
    
    try {
        for (const auto& entry : fs::directory_iterator(m_accountsRootDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                try {
                    std::uint64_t id = std::stoull(entry.path().stem().string());
                    if (id > maxId) {
                        maxId = id;
                    }
                } catch (const std::exception&) {
                    // Ignore files with non-numeric names
                    continue;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        logError("AccountStore", "Filesystem error during ID generation: " + std::string(e.what()));
    }
    
    return maxId + 1;
}

std::string AccountStore::hashPassword(const std::string& plaintext) const {
    // WARNING: This is a placeholder implementation!
    // Production MUST use bcrypt, scrypt, or Argon2 for password hashing.
    return placeholderHashPassword(plaintext);
}

} // namespace req::shared
