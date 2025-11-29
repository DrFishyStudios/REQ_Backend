#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include <vector>

#include "DataModels.h"

namespace req::shared {

/**
 * AccountStore manages account persistence to disk using JSON files.
 * Each account is stored in: data/accounts/<account_id>.json
 * 
 * This is a simple single-threaded implementation for prototyping.
 * Concurrency and advanced indexing will be added later.
 */
class AccountStore {
public:
    explicit AccountStore(const std::string& accountsRootDirectory);

    /**
     * Find an account by username.
     * Returns std::nullopt if not found.
     * Note: This performs a linear scan of all account files (naive implementation).
     */
    std::optional<data::Account> findByUsername(const std::string& username) const;

    /**
     * Load an account by ID.
     * Returns std::nullopt if not found.
     */
    std::optional<data::Account> loadById(std::uint64_t accountId) const;
    
    /**
     * Load all accounts from disk.
     * Returns a vector of all accounts (empty if none found).
     * Note: This performs a full scan of all account files.
     */
    std::vector<data::Account> loadAllAccounts() const;

    /**
     * Create a new account with the given username and plaintext password.
     * The password will be hashed before storage.
     * 
     * WARNING: The password hashing is a PLACEHOLDER and NOT cryptographically secure.
     * This must be replaced with proper bcrypt/scrypt/Argon2 in production.
     * 
     * Returns the newly created account.
     * Throws std::runtime_error if username already exists.
     */
    data::Account createAccount(const std::string& username, const std::string& passwordPlaintext);

    /**
     * Save an account to disk.
     * Returns true on success, false on failure.
     */
    bool saveAccount(const data::Account& account) const;

private:
    std::string m_accountsRootDirectory;

    /**
     * Generate a new unique account ID by scanning existing files.
     * Simple implementation: finds max ID + 1.
     */
    std::uint64_t generateNewAccountId() const;

    /**
     * Placeholder password hashing function.
     * WARNING: NOT CRYPTOGRAPHICALLY SECURE - for prototype only!
     * Production must use bcrypt, scrypt, or Argon2.
     */
    std::string hashPassword(const std::string& plaintext) const;
};

} // namespace req::shared
