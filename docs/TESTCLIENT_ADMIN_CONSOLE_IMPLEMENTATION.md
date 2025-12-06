# TestClient Admin Console - Implementation Complete

## Overview

TestClient now provides a clean admin console for dev commands with:
- Reusable `SendDevCommand` helper for UE client integration
- Admin-only access control based on login
- New `damage_self` command for HP testing
- Improved output formatting

---

## Changes Made

### 1. Added isAdmin State Tracking

**File:** `REQ_TestClient/include/req/testclient/TestClient.h`

```cpp
class TestClient {
private:
    // Session state
    req::shared::SessionToken sessionToken_;
    std::uint64_t accountId_;
    req::shared::WorldId worldId_;
    req::shared::HandoffToken handoffToken_;
    req::shared::ZoneId zoneId_;
    std::uint64_t selectedCharacterId_;
    bool isAdmin_{ false };  // NEW: Track if logged-in account is admin
```

---

### 2. Parse isAdmin from LoginResponse

**File:** `REQ_TestClient/src/TestClient.cpp`

```cpp
req::shared::protocol::LoginResponseData response;
if (!req::shared::protocol::parseLoginResponsePayload(respBody, response)) {
    // ...error handling...
}

if (!response.success) {
    // ...error handling...
}

// NEW: Store admin status
isAdmin_ = response.isAdmin;
if (isAdmin_) {
    req::shared::logInfo("TestClient", "Logged in as ADMIN account");
}
```

**Output (admin account):**
```
[INFO] [TestClient] Logged in as ADMIN account
```

**Output (regular account):**
```
(no message - isAdmin_ = false)
```

---

### 3. Created SendDevCommand Helper

**File:** `REQ_TestClient/include/req/testclient/TestClient.h`

```cpp
// NEW: Dev command helper
static void SendDevCommand(Tcp::socket& socket,
                           std::uint64_t characterId,
                           const std::string& command,
                           const std::string& param1 = "",
                           const std::string& param2 = "");
```

**File:** `REQ_TestClient/src/TestClient_Movement.cpp`

```cpp
void TestClient::SendDevCommand(Tcp::socket& socket,
                                std::uint64_t characterId,
                                const std::string& command,
                                const std::string& param1,
                                const std::string& param2) {
    req::shared::protocol::DevCommandData devCmd;
    devCmd.characterId = characterId;
    devCmd.command = command;
    devCmd.param1 = param1;
    devCmd.param2 = param2;
    
    std::string payload = req::shared::protocol::buildDevCommandPayload(devCmd);
    if (!sendMessage(socket, req::shared::MessageType::DevCommand, payload)) {
        req::shared::logError("TestClient", "Failed to send DevCommand");
    } else {
        req::shared::logInfo("TestClient", "Sent DevCommand: " + command +
            (param1.empty() ? "" : " " + param1) +
            (param2.empty() ? "" : " " + param2));
    }
}
```

**Why static:** No TestClient state needed - can be used from anywhere with a socket

**Usage:**
```cpp
SendDevCommand(socket, characterId, "suicide");
SendDevCommand(socket, characterId, "givexp", "1000");
SendDevCommand(socket, characterId, "setlevel", "10");
SendDevCommand(socket, characterId, "damage_self", "50");
SendDevCommand(socket, characterId, "respawn");
```

---

### 4. Updated Help Text to Show Admin Commands

**File:** `REQ_TestClient/src/TestClient_Movement.cpp`

```cpp
void TestClient::runMovementTestLoop(...) {
    req::shared::logInfo("TestClient", "Zone auth successful. Movement test starting.");
    
    std::cout << "\n=== Movement Test Commands ===\n";
    std::cout << "  w - Move forward\n";
    std::cout << "  s - Move backward\n";
    std::cout << "  a - Strafe left\n";
    std::cout << "  d - Strafe right\n";
    std::cout << "  j - Jump\n";
    std::cout << "  attack <npcId> - Attack an NPC\n";
    
    // NEW: Show admin commands only if logged in as admin
    if (isAdmin_) {
        std::cout << "\n--- Admin Commands ---\n";
        std::cout << "  suicide - Force character to 0 HP and trigger death\n";
        std::cout << "  givexp <amount> - Give XP to character\n";
        std::cout << "  setlevel <level> - Set character level\n";
        std::cout << "  damage_self <amount> - Apply damage to character\n";
        std::cout << "  respawn - Respawn at bind point\n";
    }
    
    std::cout << "\n  [empty] - Stop moving\n";
    std::cout << "  q - Quit movement test\n";
    std::cout << "==============================\n\n";
}
```

**Output (admin account):**
```
=== Movement Test Commands ===
  w - Move forward
  s - Move backward
  a - Strafe left
  d - Strafe right
  j - Jump
  attack <npcId> - Attack an NPC

--- Admin Commands ---
  suicide - Force character to 0 HP and trigger death
  givexp <amount> - Give XP to character
  setlevel <level> - Set character level
  damage_self <amount> - Apply damage to character
  respawn - Respawn at bind point

  [empty] - Stop moving
  q - Quit movement test
==============================
```

**Output (regular account):**
```
=== Movement Test Commands ===
  w - Move forward
  s - Move backward
  a - Strafe left
  d - Strafe right
  j - Jump
  attack <npcId> - Attack an NPC

  [empty] - Stop moving
  q - Quit movement test
==============================
```

---

### 5. Refactored All Dev Commands to Use SendDevCommand

**Before:**
```cpp
// Old suicide code (duplicated pattern)
if (command == "suicide") {
    req::shared::protocol::DevCommandData devCmd;
    devCmd.characterId = localCharacterId;
    devCmd.command = "suicide";
    devCmd.param1 = "";
    devCmd.param2 = "";
    
    std::string payload = req::shared::protocol::buildDevCommandPayload(devCmd);
    if (!sendMessage(*zoneSocket, req::shared::MessageType::DevCommand, payload)) {
        req::shared::logError("TestClient", "Failed to send DevCommand");
    } else {
        req::shared::logInfo("TestClient", "Sent DevCommand: suicide");
    }
    continue;
}
```

**After:**
```cpp
// New suicide code (clean and simple)
if (command == "suicide") {
    if (!isAdmin_) {
        std::cout << "[DEV] ERROR: Dev commands require an admin account\n";
        continue;
    }
    
    SendDevCommand(*zoneSocket, localCharacterId, "suicide");
    continue;
}
```

**Benefits:**
- Eliminates code duplication
- Consistent error handling
- Reusable for UE client
- Easier to maintain

---

### 6. Added Admin Access Control

**Every dev command now checks admin status:**

```cpp
if (command == "suicide") {
    if (!isAdmin_) {
        std::cout << "[DEV] ERROR: Dev commands require an admin account\n";
        continue;
    }
    
    SendDevCommand(*zoneSocket, localCharacterId, "suicide");
    continue;
}
```

**Error message shown to non-admin:**
```
[DEV] ERROR: Dev commands require an admin account
```

**Commands protected:**
- ? `suicide`
- ? `givexp <amount>`
- ? `setlevel <level>`
- ? `damage_self <amount>` (NEW)
- ? `respawn`

---

### 7. Added damage_self Command

```cpp
// NEW: Check for dev command: damage_self
if (command.find("damage_self ") == 0) {
    if (!isAdmin_) {
        std::cout << "[DEV] ERROR: Dev commands require an admin account\n";
        continue;
    }
    
    std::string amountStr = command.substr(12); // Skip "damage_self "
    try {
        std::int32_t amount = std::stoi(amountStr);
        if (amount <= 0) {
            std::cout << "Damage amount must be positive. Usage: damage_self <amount>\n";
        } else {
            SendDevCommand(*zoneSocket, localCharacterId, "damage_self", amountStr);
        }
    } catch (const std::exception& e) {
        std::cout << "Invalid damage amount: '" << amountStr << "'. Usage: damage_self <amount>\n";
    }
    continue;
}
```

**Usage:** `damage_self 50`

**Validates:**
- ? Amount must be a valid integer
- ? Amount must be positive (> 0)
- ? Requires admin account

**Error messages:**
- `Invalid damage amount: 'abc'. Usage: damage_self <amount>` (parse error)
- `Damage amount must be positive. Usage: damage_self <amount>` (zero or negative)
- `[DEV] ERROR: Dev commands require an admin account` (non-admin)

---

### 8. Improved DevCommandResponse Output

**Before:**
```cpp
if (parseDevCommandResponsePayload(msgBody, response)) {
    std::cout << "[CLIENT] DevCommand " << (response.success ? "SUCCESS" : "FAILED")
             << ": " << response.message << std::endl;
}
```

**After:**
```cpp
if (parseDevCommandResponsePayload(msgBody, response)) {
    if (response.success) {
        std::cout << "[DEV] OK: " << response.message << std::endl;
    } else {
        std::cout << "[DEV] ERROR: " << response.message << std::endl;
    }
}
```

**Output examples:**

Success:
```
[DEV] OK: Applied 50 damage
[DEV] OK: Gave 1000 XP
[DEV] OK: Set level to 10
[DEV] OK: Character forced to 0 HP and death triggered
[DEV] OK: Player respawned at bind point
```

Error:
```
[DEV] ERROR: Dev commands require an admin account
[DEV] ERROR: Player not found in zone
[DEV] ERROR: Invalid damage amount: abc
```

---

## Complete Dev Command List

| Command | Syntax | Admin Only | Description |
|---------|--------|------------|-------------|
| `suicide` | `suicide` | ? | Sets HP to 0, triggers death |
| `givexp` | `givexp <amount>` | ? | Awards XP, handles level-ups |
| `setlevel` | `setlevel <level>` | ? | Sets level and XP directly |
| `damage_self` | `damage_self <amount>` | ? **NEW** | Subtracts HP, triggers death if 0 |
| `respawn` | `respawn` | ? | Respawns player at bind point |

**All commands:**
- ? Protected by admin check (client-side)
- ? Protected by admin check (server-side)
- ? Send `DevCommandResponse` with success/failure
- ? Log to TestClient console
- ? Reject invalid input gracefully

---

## Testing

### Test 1: Admin Account (Success)

**Login:**
```sh
.\REQ_TestClient.exe
# Enter username: admin
# Enter password: AdminPass123!
```

**Expected Output:**
```
[INFO] [TestClient] Logged in as ADMIN account
```

**Help shows admin commands:**
```
=== Movement Test Commands ===
  w - Move forward
  s - Move backward
  a - Strafe left
  d - Strafe right
  j - Jump
  attack <npcId> - Attack an NPC

--- Admin Commands ---
  suicide - Force character to 0 HP and trigger death
  givexp <amount> - Give XP to character
  setlevel <level> - Set character level
  damage_self <amount> - Apply damage to character
  respawn - Respawn at bind point

  [empty] - Stop moving
  q - Quit movement test
==============================
```

**Test commands:**
```
Movement command: damage_self 50
[INFO] [TestClient] Sent DevCommand: damage_self 50
[DEV] OK: Applied 50 damage

Movement command: givexp 1000
[INFO] [TestClient] Sent DevCommand: givexp 1000
[DEV] OK: Gave 1000 XP

Movement command: setlevel 10
[INFO] [TestClient] Sent DevCommand: setlevel 10
[DEV] OK: Set level to 10

Movement command: suicide
[INFO] [TestClient] Sent DevCommand: suicide
[DEV] OK: Character forced to 0 HP and death triggered

Movement command: respawn
[INFO] [TestClient] Sent DevCommand: respawn
[DEV] OK: Player respawned at bind point
```

---

### Test 2: Non-Admin Account (Rejected)

**Login:**
```sh
.\REQ_TestClient.exe
# Enter username: testuser
# Enter password: testpass
```

**Expected Output:**
```
(No "Logged in as ADMIN account" message)
```

**Help hides admin commands:**
```
=== Movement Test Commands ===
  w - Move forward
  s - Move backward
  a - Strafe left
  d - Strafe right
  j - Jump
  attack <npcId> - Attack an NPC

  [empty] - Stop moving
  q - Quit movement test
==============================
```

**Test commands (all rejected):**
```
Movement command: damage_self 50
[DEV] ERROR: Dev commands require an admin account

Movement command: givexp 1000
[DEV] ERROR: Dev commands require an admin account

Movement command: suicide
[DEV] ERROR: Dev commands require an admin account
```

**Commands never sent to server** - rejected client-side for security and UX.

---

### Test 3: damage_self Command Validation

**Admin account:**

```
Movement command: damage_self 50
[INFO] [TestClient] Sent DevCommand: damage_self 50
[DEV] OK: Applied 50 damage

Movement command: damage_self 0
Damage amount must be positive. Usage: damage_self <amount>

Movement command: damage_self -10
Damage amount must be positive. Usage: damage_self <amount>

Movement command: damage_self abc
Invalid damage amount: 'abc'. Usage: damage_self <amount>

Movement command: damage_self 100
[INFO] [TestClient] Sent DevCommand: damage_self 100
[DEV] OK: damage_self killed the player
```

---

## UE Client Integration

### How to Reuse SendDevCommand

**File:** `YourUEClientNetworking.cpp`

```cpp
// Copy the helper function signature
void SendDevCommand(boost::asio::ip::tcp::socket& socket,
                    std::uint64_t characterId,
                    const std::string& command,
                    const std::string& param1 = "",
                    const std::string& param2 = "") {
    req::shared::protocol::DevCommandData devCmd;
    devCmd.characterId = characterId;
    devCmd.command = command;
    devCmd.param1 = param1;
    devCmd.param2 = param2;
    
    std::string payload = req::shared::protocol::buildDevCommandPayload(devCmd);
    // ... send payload via your socket ...
}

// Usage from UE client
if (IsAdmin) {
    SendDevCommand(ZoneSocket, LocalCharacterId, "damage_self", "50");
}
```

**Or integrate into your networking layer:**

```cpp
class ZoneConnection {
public:
    void SendDevCommand(const std::string& command,
                       const std::string& param1 = "",
                       const std::string& param2 = "") {
        req::shared::protocol::DevCommandData devCmd;
        devCmd.characterId = myCharacterId;
        devCmd.command = command;
        devCmd.param1 = param1;
        devCmd.param2 = param2;
        
        std::string payload = req::shared::protocol::buildDevCommandPayload(devCmd);
        SendMessage(MessageType::DevCommand, payload);
    }
};

// Usage
if (PlayerIsAdmin) {
    zoneConnection.SendDevCommand("damage_self", "50");
}
```

---

## File Summary

### Files Modified

1. **REQ_TestClient/include/req/testclient/TestClient.h**
   - Added `isAdmin_` member
   - Added `SendDevCommand` static helper declaration

2. **REQ_TestClient/src/TestClient.cpp**
   - Parse `isAdmin` from `LoginResponse`
   - Log when admin account detected
   - Store `isAdmin_` state

3. **REQ_TestClient/src/TestClient_Movement.cpp**
   - Implemented `SendDevCommand` helper
   - Updated help text to conditionally show admin commands
   - Refactored all dev commands to use helper
   - Added admin check before each dev command
   - Added new `damage_self` command
   - Improved `DevCommandResponse` output formatting

**Total:** ~100 lines added/modified

---

## Build Status

? **Build successful** - All projects compiled without errors

---

## Benefits

### For TestClient Users

? **Clear admin indication** - Know immediately if you're admin
? **Conditional help** - Only see commands you can use
? **Client-side protection** - Fast feedback for non-admin attempts
? **Consistent output** - All dev commands use same format
? **New testing capability** - `damage_self` for HP testing

### For Developers

? **Reusable helper** - Easy to integrate into UE client
? **No code duplication** - All dev commands use same pattern
? **Maintainable** - Changes to dev command protocol in one place
? **Extensible** - Easy to add new dev commands

### For Server

? **Double protection** - Client + server both check admin
? **Reduced traffic** - Non-admin commands never sent
? **Better logging** - Clear separation of admin actions

---

## Next Steps

### Short-term

1. **Add more dev commands:**
   - `teleport <x> <y> <z>` - Instant position change
   - `spawn_npc <npcId>` - Spawn NPC at player location
   - `give_item <itemId>` - Add item to inventory
   - `set_hp <amount>` - Set HP directly (not damage)
   - `godmode` - Toggle invincibility

2. **Command history:**
   - Up/down arrow to repeat commands
   - Command aliases (e.g., `gxp` for `givexp`)

3. **Batch commands:**
   - `damage_self 10; damage_self 10; damage_self 10`
   - Semicolon-separated commands

### Long-term

1. **UE Client Admin Panel:**
   - GUI for dev commands
   - Dropdown for common values
   - Real-time HP/XP display

2. **Command Logging:**
   - Audit log for all admin commands
   - Timestamp, account, character, command, result
   - Queryable for investigations

3. **Permission Levels:**
   - Different admin tiers (admin, moderator, GM)
   - Fine-grained command permissions
   - Role-based access control

---

## Conclusion

TestClient now provides a **clean, admin-only console** for dev commands with:

? Reusable `SendDevCommand` helper
? Admin-only access control (client + server)
? New `damage_self` command
? Conditional help text
? Improved output formatting
? Ready for UE client integration

**All dev commands are now:**
- Hidden from non-admin users
- Protected by dual checks (client + server)
- Easy to use and maintain
- Reusable across clients

**Status:** ? **COMPLETE - Ready for testing!** ??
