# Account JSON File Generation Guide

## Overview
This guide explains how to create valid account JSON files for the REQ_Backend LoginServer.

---

## A) Account JSON Schema

### Field Definitions

| Field Name | Type | Required | Default | Description |
|---|---|---|---|---|
| `account_id` | uint64 | Yes | - | Unique account identifier (file name must match) |
| `username` | string | Yes | - | Login username (case-sensitive) |
| `password_hash` | string | Yes | - | Hashed password using placeholder hash function |
| `is_banned` | boolean | No | false | Whether account is banned |
| `is_admin` | boolean | No | false | Whether account has admin privileges |
| `display_name` | string | No | "" | Display name (defaults to username if empty) |
| `email` | string | No | "" | Email address |

### Example JSON Structure

```json
{
    "account_id": 1,
    "username": "Aradune",
    "password_hash": "PLACEHOLDER_HASH_1234567890",
    "is_banned": false,
    "is_admin": false,
    "display_name": "Aradune Mithara",
    "email": "aradune@example.com"
}
```

---

## B) Automatic Account Generation (RECOMMENDED)

### Method 1: Using the Built-in Helper

The easiest way to create test accounts is to use the built-in helper:

```powershell
# From the x64/Debug directory (or x64/Release for release builds)
cd x64\Debug
.\REQ_LoginServer.exe --create-test-accounts
```

This will automatically create the following test accounts in `data/accounts/`:

| Username | Password | Admin | Description |
|---|---|---|---|
| `testuser` | `testpass` | No | Basic test account |
| `Aradune` | `TestPassword123!` | No | EQ reference account |
| `admin` | `AdminPass123!` | Yes | Administrator account |
| `player1` | `password123` | No | Additional test account |

**Example Output:**
```
[INFO] [CreateTestAccounts] === Creating Test Accounts ===
[INFO] [CreateTestAccounts] Processing account: testuser
[INFO] [CreateTestAccounts]   ? Created account 'testuser' (ID: 1)
[INFO] [CreateTestAccounts]     Password: testpass
[INFO] [CreateTestAccounts]     Display Name: Test User
[INFO] [CreateTestAccounts] Processing account: Aradune
[INFO] [CreateTestAccounts]   ? Created account 'Aradune' (ID: 2)
[INFO] [CreateTestAccounts]     Password: TestPassword123!
[INFO] [CreateTestAccounts]     Display Name: Aradune Mithara
...
[INFO] [CreateTestAccounts] === Summary ===
[INFO] [CreateTestAccounts]   Created: 4
[INFO] [CreateTestAccounts]   Skipped (already exist): 0
```

**Notes:**
- The utility will skip accounts that already exist (based on username)
- Account IDs are auto-generated starting from 1
- All passwords are properly hashed using the same function LoginServer uses
- The `data/accounts` directory is created automatically if it doesn't exist

---

## C) Manual Account JSON Generation

If you prefer to create account files manually, you need to generate the correct password hash.

### Password Hash Algorithm

The current implementation uses a **placeholder hash** (NOT cryptographically secure):

```cpp
std::hash<std::string> hasher;
std::size_t hashValue = hasher(plaintext + "_salt_placeholder");
std::string result = "PLACEHOLDER_HASH_" + std::to_string(hashValue);
```

**?? WARNING:** This is for development/testing only! Production must use bcrypt, scrypt, or Argon2.

### Pre-Computed Password Hashes

Here are some common test passwords with their hashes (these are platform/compiler-specific):

| Plaintext Password | Hash (Visual Studio 2022, x64) |
|---|---|
| `testpass` | `PLACEHOLDER_HASH_<varies>` |
| `TestPassword123!` | `PLACEHOLDER_HASH_<varies>` |
| `password123` | `PLACEHOLDER_HASH_<varies>` |

**Important:** Password hashes vary by platform/compiler. Use the helper utility (Method B) instead.

### Manual File Creation (Not Recommended)

If you must create files manually:

1. Use the helper utility to create one account
2. Look at the generated JSON in `data/accounts/1.json`
3. Copy the `password_hash` value from that file
4. Use it for other accounts with the same password

**Example: data/accounts/1.json**
```json
{
    "account_id": 1,
    "username": "Aradune",
    "password_hash": "PLACEHOLDER_HASH_18446744073709551615",
    "is_banned": false,
    "is_admin": false,
    "display_name": "Aradune",
    "email": ""
}
```

**File Naming Convention:**
- Filename MUST be: `<account_id>.json`
- Example: Account ID 1 ? `1.json`, Account ID 42 ? `42.json`
- File must be placed in: `data/accounts/` (relative to executable)

---

## D) LoginServer Authentication Behavior

### With Test Accounts Created

Once you've created accounts using the helper utility, you can login with:

**Example 1: Basic Test Account**
```
Username: testuser
Password: testpass
```

**Example 2: Aradune Account**
```
Username: Aradune
Password: TestPassword123!
```

**Example 3: Admin Account**
```
Username: admin
Password: AdminPass123!
```

### Authentication Flow

When TestClient (or any client) sends a LoginRequest:

1. **LoginServer receives request** with username and plaintext password
2. **FindByUsername** searches `data/accounts/*.json` files for matching username
3. **Password verification:**
   - Takes plaintext password from request
   - Hashes it using same algorithm: `placeholderHashPassword(plaintext)`
   - Compares hash with `password_hash` field in account JSON
4. **If hashes match:** Login succeeds, session token generated
5. **If hashes don't match:** Login fails with `INVALID_PASSWORD` error

### Error Cases

| Scenario | Error Code | Error Message |
|---|---|---|
| Username not found | `ACCOUNT_NOT_FOUND` | "Invalid username or password" |
| Wrong password | `INVALID_PASSWORD` | "Invalid username or password" |
| Account banned | `ACCOUNT_BANNED` | "This account has been banned" |
| Empty username | `INVALID_USERNAME` | "Username cannot be empty" |

---

## E) Testing the Accounts

### Step 1: Create Test Accounts
```powershell
cd x64\Debug
.\REQ_LoginServer.exe --create-test-accounts
```

### Step 2: Start LoginServer
```powershell
# In same terminal or new terminal
.\REQ_LoginServer.exe
```

### Step 3: Update TestClient
Modify `REQ_TestClient/src/TestClient.cpp` to use a valid account:

```cpp
const std::string username = "Aradune";        // Changed from "testuser"
const std::string password = "TestPassword123!"; // Changed from "testpass"
```

Or use registration mode to create a new account:

```cpp
// In doLogin, change the mode parameter
std::string requestPayload = req::shared::protocol::buildLoginRequestPayload(
    username, password, clientVersion, req::shared::protocol::LoginMode::Register);
```

### Step 4: Run TestClient
```powershell
cd x64\Debug
.\REQ_TestClient.exe
```

**Expected Success Output:**
```
[INFO] [TestClient] Sending LoginRequest: username=Aradune, clientVersion=0.1.0, mode=login
[INFO] [TestClient] Login stage succeeded:
[INFO] [TestClient]   sessionToken=123456789
[INFO] [TestClient]   worldId=1
```

**Expected Failure Output (wrong password):**
```
[ERROR] [TestClient] Login failed: INVALID_PASSWORD - Invalid username or password
```

---

## F) Directory Structure

After running `--create-test-accounts`, your directory structure should look like:

```
REQ_Backend/
??? x64/
?   ??? Debug/
?       ??? REQ_LoginServer.exe
?       ??? REQ_TestClient.exe
?       ??? data/
?           ??? accounts/
?               ??? 1.json          # testuser account
?               ??? 2.json          # Aradune account
?               ??? 3.json          # admin account
?               ??? 4.json          # player1 account
```

Each JSON file contains a properly formatted account with correct password hashes.

---

## G) Quick Reference

### Create Test Accounts
```powershell
.\REQ_LoginServer.exe --create-test-accounts
```

### Login Credentials (after creation)
- `testuser` / `testpass`
- `Aradune` / `TestPassword123!`
- `admin` / `AdminPass123!`
- `player1` / `password123`

### File Location
`data/accounts/<account_id>.json`

### JSON Template
```json
{
    "account_id": 1,
    "username": "YourUsername",
    "password_hash": "PLACEHOLDER_HASH_12345",
    "is_banned": false,
    "is_admin": false,
    "display_name": "Your Display Name",
    "email": "your@email.com"
}
```

---

## H) Troubleshooting

### "Account not found" error
- Make sure the JSON file exists in `data/accounts/`
- Check that the username in the file matches exactly (case-sensitive)
- Verify the file is named correctly: `<account_id>.json`

### "Invalid password" error
- Make sure you used the helper utility to generate the account
- Password hashes are platform-specific - don't copy between machines
- Check that you're using the exact password that was used during creation

### "Username already exists" when creating
- Delete the existing account file in `data/accounts/` first, OR
- Use a different username

### File permissions
- Make sure the `data/accounts/` directory is writable
- On Windows, run as Administrator if needed

---

## I) Future Improvements

The current password hashing is a **placeholder** and should be replaced with proper cryptographic hashing:

```cpp
// TODO: Replace placeholderHashPassword with:
// - bcrypt (recommended)
// - scrypt
// - Argon2
```

This will require:
1. Adding a crypto library (e.g., OpenSSL, libsodium)
2. Updating `AccountStore::hashPassword()`
3. Migrating existing account files (or resetting passwords)
