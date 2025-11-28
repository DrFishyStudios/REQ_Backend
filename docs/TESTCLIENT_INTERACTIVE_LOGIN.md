# TestClient Interactive Login Guide

## Overview

The REQ_TestClient now supports interactive login prompts, allowing you to:
- Enter custom username and password for each test run
- Choose between login and registration modes
- Use default credentials for quick testing (just press Enter)

---

## Quick Start

### Using Default Credentials

The fastest way to test is to just press Enter for all prompts:

```powershell
cd x64\Debug
.\REQ_TestClient.exe

# Press Enter 3 times to use defaults:
# - Username: testuser
# - Password: testpass  
# - Mode: login
```

This works if you've already created the `testuser` account using:
```powershell
.\REQ_LoginServer.exe --create-test-accounts
```

### Custom Login

```powershell
.\REQ_TestClient.exe

# Enter custom credentials:
Enter username (default: testuser): Aradune
Enter password (default: testpass): TestPassword123!
Mode [login/register] (default: login): login
```

### Registering a New Account

```powershell
.\REQ_TestClient.exe

# Register a new account:
Enter username (default: testuser): MyNewUser
Enter password (default: testpass): MySecurePass!
Mode [login/register] (default: login): register
```

On success, the account is created and you're logged in automatically.

---

## Interactive Prompts

### Username Prompt
```
Enter username (default: testuser):
```

- **Press Enter:** Uses default `testuser`
- **Type username:** Uses your custom username
- **Case-sensitive:** `Aradune` ? `aradune`

### Password Prompt
```
Enter password (default: testpass):
```

- **Press Enter:** Uses default `testpass`
- **Type password:** Uses your custom password
- **Note:** Password is visible on screen (dev tool only!)

### Mode Prompt
```
Mode [login/register] (default: login):
```

Valid inputs (case-insensitive):
- **`login`** or **press Enter:** Login with existing account
- **`register`** or **`reg`** or **`r`:** Create new account

Invalid inputs default to `login` mode.

---

## Login Modes

### Login Mode (Default)

Authenticates with an existing account.

**Example:**
```
Enter username (default: testuser): testuser
Enter password (default: testpass): testpass
Mode [login/register] (default: login): [Enter]
```

**Expected Output:**
```
[INFO] [TestClient] Logging in with existing account: username=testuser
[INFO] [TestClient] Mode: login
[INFO] [TestClient] --- Stage 1: Login/Registration ---
[INFO] [TestClient] Connecting to login server at 127.0.0.1:7777...
[INFO] [TestClient] Connected to login server
[INFO] [TestClient] Sending LoginRequest: username=testuser, clientVersion=REQ-TestClient-0.1, mode=login
[INFO] [TestClient] Login succeeded!
[INFO] [TestClient]   sessionToken=123456789
[INFO] [TestClient]   worldId=1
```

**Common Errors:**

| Error Code | Cause | Solution |
|---|---|---|
| `ACCOUNT_NOT_FOUND` | Username doesn't exist | Use register mode or check spelling |
| `INVALID_PASSWORD` | Wrong password | Check password spelling |
| `ACCOUNT_BANNED` | Account is banned | Contact administrator or use different account |

### Register Mode

Creates a new account and logs in automatically.

**Example:**
```
Enter username (default: testuser): Aradune
Enter password (default: testpass): MyPassword123!
Mode [login/register] (default: login): register
```

**Expected Output:**
```
[INFO] [TestClient] Registering new account: username=Aradune
[INFO] [TestClient] Mode: register
[INFO] [TestClient] --- Stage 1: Login/Registration ---
[INFO] [TestClient] Connecting to login server at 127.0.0.1:7777...
[INFO] [TestClient] Connected to login server
[INFO] [TestClient] Sending LoginRequest: username=Aradune, clientVersion=REQ-TestClient-0.1, mode=register
[INFO] [TestClient] Registration and login succeeded!
[INFO] [TestClient]   sessionToken=987654321
[INFO] [TestClient]   worldId=1
```

**Common Errors:**

| Error Code | Cause | Solution |
|---|---|---|
| `USERNAME_TAKEN` | Username already exists | Use login mode or choose different username |
| `INVALID_USERNAME` | Empty or invalid username | Provide a valid username |

---

## Complete Test Session Examples

### Example 1: First-Time User (Register ? Play)

```powershell
# Terminal 1: Start servers
cd x64\Debug
.\REQ_LoginServer.exe

# Terminal 2
.\REQ_WorldServer.exe

# Terminal 3
.\REQ_ZoneServer.exe

# Terminal 4: Run TestClient
.\REQ_TestClient.exe
```

**TestClient Interaction:**
```
--- Login Information ---
Enter username (default: testuser): MyHero
Enter password (default: testpass): SecretPass123
Mode [login/register] (default: login): register

[INFO] [TestClient] Registering new account: username=MyHero
[INFO] [TestClient] Mode: register
[INFO] [TestClient] Registration and login succeeded!
[INFO] [TestClient] --- Stage 2: Character List ---
[INFO] [TestClient] Character list retrieved: 0 character(s)
[INFO] [TestClient] No characters found. Creating a new character...
[INFO] [TestClient] Character created: id=1, name=TestWarrior, race=Human, class=Warrior, level=1
[INFO] [TestClient] --- Stage 3: Enter World ---
[INFO] [TestClient] Selecting character: id=1, name=TestWarrior
[INFO] [TestClient] --- Stage 4: Zone Auth ---
[INFO] [TestClient] Zone entry successful: Welcome to East Freeport
[INFO] [TestClient] === Full Handshake Completed Successfully ===
```

### Example 2: Returning User (Login ? Play)

```powershell
.\REQ_TestClient.exe
```

**TestClient Interaction:**
```
--- Login Information ---
Enter username (default: testuser): MyHero
Enter password (default: testpass): SecretPass123
Mode [login/register] (default: login): login

[INFO] [TestClient] Logging in with existing account: username=MyHero
[INFO] [TestClient] Mode: login
[INFO] [TestClient] Login succeeded!
[INFO] [TestClient] --- Stage 2: Character List ---
[INFO] [TestClient] Character list retrieved: 1 character(s)
[INFO] [TestClient]   Character: id=1, name=TestWarrior, race=Human, class=Warrior, level=1
[INFO] [TestClient] --- Stage 3: Enter World ---
[INFO] [TestClient] === Full Handshake Completed Successfully ===
```

### Example 3: Quick Test (Default Credentials)

```powershell
# First, ensure test account exists:
.\REQ_LoginServer.exe --create-test-accounts

# Then run TestClient with defaults:
.\REQ_TestClient.exe
[Enter]  # Use default username
[Enter]  # Use default password
[Enter]  # Use default mode (login)
```

---

## Default Credentials

The following defaults are configured for quick testing:

| Setting | Default Value |
|---|---|
| Username | `testuser` |
| Password | `testpass` |
| Mode | `login` |
| Client Version | `REQ-TestClient-0.1` |

To use defaults, simply press **Enter** when prompted.

**Note:** The `testuser` account must exist first. Create it with:
```powershell
.\REQ_LoginServer.exe --create-test-accounts
```

---

## Security Notes

### ?? Development Tool Only

**Important:**
- Passwords are displayed on screen (not hidden)
- No input masking (e.g., `****`)
- Not suitable for production use

This is intentional for a **development/testing tool**. Real clients should:
- Hide password input
- Use secure connection (TLS/SSL)
- Implement proper credential storage

### Password Security

- Passwords are **never logged** to the log files
- Only username and mode are logged
- Passwords are transmitted to server but not written to disk by TestClient

**Log Example (Safe):**
```
[INFO] [TestClient] Logging in with existing account: username=Aradune
[INFO] [TestClient] Mode: login
[INFO] [TestClient] Sending LoginRequest: username=Aradune, clientVersion=REQ-TestClient-0.1, mode=login
```

Notice: Password is **not** in the logs.

---

## Troubleshooting

### Issue: TestClient Exits Immediately

**Cause:** Login failed or server not running

**Solution:**
1. Check if LoginServer is running on port 7777
2. Verify credentials are correct
3. Check logs for error messages
4. TestClient now pauses on error - check console output

### Issue: "Press Enter to exit..." Appears Too Quickly

**Cause:** Error occurred during handshake

**Solution:**
- Read the error messages before the pause
- Check server logs for detailed error information
- Verify all servers (Login, World, Zone) are running

### Issue: Can't Enter Username/Password

**Cause:** Console input buffering

**Solution:**
- Make sure console window has focus
- Try clicking in the console window
- On some terminals, press Enter first to clear buffer

### Issue: Mode Input Doesn't Work

**Valid inputs:**
- `login`, `Login`, `LOGIN` ? Login mode
- `register`, `Register`, `reg`, `r` ? Register mode
- Invalid input ? Defaults to login mode

---

## Advanced Usage

### Automated Testing with Defaults

For scripts or automated testing:

```powershell
# Use echo to pipe Enter keypresses
(echo. & echo. & echo.) | .\REQ_TestClient.exe
```

This automatically uses all default values.

### Custom Character Names

Currently, character creation uses hardcoded values:
- Name: `TestWarrior`
- Race: `Human`
- Class: `Warrior`

To customize, modify `TestClient.cpp`:
```cpp
// In run() method, character creation section
if (!doCharacterCreate(worldHost, worldPort, sessionToken, worldId, 
                      "MyCustomName", "HighElf", "Wizard", newChar)) {
```

Future enhancement: Add interactive character creation prompts.

---

## Testing Different Scenarios

### Scenario 1: Test Registration Error (Duplicate Username)

```
Run 1:
  Username: TestDupe
  Password: pass123
  Mode: register
  ? Success

Run 2:
  Username: TestDupe
  Password: pass123
  Mode: register
  ? Error: USERNAME_TAKEN
```

### Scenario 2: Test Login Error (Wrong Password)

```
Setup: Create account via --create-test-accounts

Test:
  Username: testuser
  Password: wrongpassword
  Mode: login
  ? Error: INVALID_PASSWORD
```

### Scenario 3: Test Multiple Accounts

```
Run 1: Register "Player1" / "pass1"
Run 2: Register "Player2" / "pass2"
Run 3: Login as "Player1"
Run 4: Login as "Player2"
```

Each account maintains separate characters and progress.

---

## What's Next

After successful login and character creation, TestClient automatically:

1. ? Connects to WorldServer
2. ? Retrieves character list
3. ? Creates character if none exist
4. ? Selects first character
5. ? Requests zone entry
6. ? Connects to ZoneServer
7. ? Enters zone

All stages are fully automated after initial login prompts.

---

## Summary

**Quick Test:**
```powershell
.\REQ_TestClient.exe
[Enter] [Enter] [Enter]  # Use defaults
```

**Custom Login:**
```powershell
.\REQ_TestClient.exe
Username: Aradune
Password: TestPassword123!
Mode: login
```

**New Account:**
```powershell
.\REQ_TestClient.exe
Username: NewPlayer
Password: MyPass123
Mode: register
```

**Now you have full control over login credentials for each test run!**
