# REQ_Backend Client Core Design (ReqClientCore)
## Audit & API Design

**Status:** Design-only – no code changes yet  
**Date:** 2024  
**Author:** REQ_Backend Team

---

## Executive Summary

The console **TestClient** project contains substantial networking, protocol, and handshake logic that should be shared with the new **REQ_VizTestClient** (SFML) and future **Unreal Engine client**. This document:

1. **Inventories** which files contain core functionality vs UI-only code
2. **Proposes** a new static library **ReqClientCore** with a minimal C++ API
3. **Maps** existing capabilities to the new API

**Goal:** Create a shared client networking library that eliminates code duplication and provides a clean API for all REQ client applications.

---

## 1. Existing TestClient File Inventory

### 1.1 Core Networking & Protocol (? ReqClientCore)

These files implement the handshake flow and protocol logic, independent of console UI:

| File | Purpose | Core Functionality |
|------|---------|-------------------|
| `TestClient.h/cpp` | Main handshake orchestration | `doLogin()`, `doCharacterList()`, `doCharacterCreate()`, `doEnterWorld()`, `doZoneAuthAndConnect()` – **all reusable** |
| `TestClient_Movement.cpp` | Zone connection & movement loop | `doZoneAuthAndConnect()`, `SendDevCommand()` helper, message send/receive – **core networking** |
| `TestClient_Scenarios*.cpp` | Automated test scenarios | Uses core methods; scenarios themselves are **test harness only** |
| `BotClient.h/cpp` | Automated bot for load testing | Complete handshake flow, movement AI – **reusable as example/reference** |
| `BotManager.h/cpp` | Multi-bot orchestration | Uses BotClient; **test harness only** |

**Key observation:** All handshake methods (`doLogin`, `doCharacterList`, `doEnterWorld`, etc.) are **self-contained** and **UI-agnostic** – they only depend on Boost.Asio and REQ_Shared protocol definitions.

### 1.2 Console UI & Test Harness (? Stay in TestClient)

These files handle user interaction and test automation specific to the console client:

| File | Purpose | TestClient-Only |
|------|---------|----------------|
| `main.cpp` | Menu system, CLI args | Interactive menu, scenario selection |
| `TestClient_Scenarios_HappyPath.cpp` | Happy path automation | Console prompts, `std::cout` logging |
| `TestClient_Scenarios_Negative.cpp` | Error case testing | Intentionally invalid payloads, test assertions |
| `ClientStages.h` | Stage enum for logging | `EClientStage` – useful for tracking but not required for core |

**Key observation:** All console I/O (`std::cin`, `std::cout`, test menus) stays in TestClient. Core networking has **zero dependencies** on these.

### 1.3 Protocol & Message Handling (Already in REQ_Shared)

The protocol layer is **already shared** via REQ_Shared:

- `MessageHeader.h` – Binary message header
- `MessageTypes.h` – `MessageType` enum
- `ProtocolSchemas.h` + `.cpp` – Payload builders/parsers for:
  - Login/World handshake (`LoginRequest/Response`, `CharacterListRequest/Response`, etc.)
  - Zone handshake (`ZoneAuthRequest/Response`)
  - Movement (`MovementIntent`, `PlayerStateSnapshot`)
  - Combat (`AttackRequest/Result`)
  - Dev commands (`DevCommand/Response`)
- `Connection.h` – Async Boost.Asio connection wrapper

**These are already shared** – no duplication needed. ReqClientCore will **use** these, not re-implement them.

---

## 2. What Goes Into ReqClientCore?

### 2.1 Core Responsibilities

ReqClientCore will be a **static library** that provides:

1. **Login handshake** – Connect to LoginServer, authenticate, retrieve world list
2. **World handshake** – Connect to WorldServer, get character list, create characters, enter world
3. **Zone handshake** – Connect to ZoneServer with handoff token, enter zone
4. **Movement intent** – Send `MovementIntent` messages to zone
5. **Zone state reception** – Receive and parse `PlayerStateSnapshot` messages
6. **Combat requests** – Send `AttackRequest` messages
7. **Dev commands** – Send `DevCommand` messages (admin only)
8. **Session state tracking** – Store `sessionToken`, `worldId`, `handoffToken`, `zoneId`, `characterId`, `isAdmin`

**What it does NOT do:**
- ? Console I/O (`std::cin`/`std::cout`)
- ? Test automation (`runHappyPathScenario`, etc.)
- ? Bot AI (movement patterns, auto-combat)
- ? SFML rendering
- ? Unreal Engine integration

**Design goal:** Provide a **minimal, stateless API** that both console TestClient and SFML/UE clients can call.

---

## 3. Proposed API (ReqClientCore Public Interface)

### 3.1 Header: `include/req/clientcore/ClientCore.h`

```cpp
#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <vector>

#include <boost/asio.hpp>

#include "../../REQ_Shared/include/req/shared/Types.h"
#include "../../REQ_Shared/include/req/shared/ProtocolSchemas.h"
#include "../../REQ_Shared/include/req/shared/MessageTypes.h"

namespace req::clientcore {

// ============================================================================
// Configuration
// ============================================================================

struct ClientConfig {
    std::string clientVersion{ "REQ-ClientCore-1.0" };
    std::string loginServerHost{ "127.0.0.1" };
    std::uint16_t loginServerPort{ 7777 };
    // Future: timeout settings, retry logic, logging callbacks
};

// ============================================================================
// Session State (Opaque Handle)
// ============================================================================

/**
 * ClientSession
 * 
 * Tracks current session state across Login ? World ? Zone handshake.
 * This is an opaque data structure - clients should not modify fields directly.
 * All operations on the session are performed via free functions in req::clientcore.
 */
struct ClientSession {
    // Login state
    req::shared::SessionToken sessionToken{ 0 };
    std::uint64_t accountId{ 0 };
    bool isAdmin{ false };
    
    // World state
    req::shared::WorldId worldId{ 0 };
    std::string worldHost;
    std::uint16_t worldPort{ 0 };
    
    // Zone state
    req::shared::HandoffToken handoffToken{ 0 };
    req::shared::ZoneId zoneId{ 0 };
    std::string zoneHost;
    std::uint16_t zonePort{ 0 };
    std::uint64_t selectedCharacterId{ 0 };
    
    // Persistent zone connection (managed by connectToZone/disconnectFromZone)
    std::shared_ptr<boost::asio::io_context> zoneIoContext;
    std::shared_ptr<boost::asio::ip::tcp::socket> zoneSocket;
};

// ============================================================================
// Login Handshake
// ============================================================================

enum class LoginResult {
    Success,
    ConnectionFailed,
    InvalidCredentials,
    AccountBanned,
    NoWorldsAvailable,
    ProtocolError
};

struct LoginResponse {
    LoginResult result;
    std::string errorMessage;  // Human-readable error (if failed)
    std::vector<req::shared::protocol::WorldEntry> availableWorlds;  // If success
};

/**
 * login
 * 
 * Connects to LoginServer and authenticates with username/password.
 * On success, populates session with sessionToken, worldId, worldHost/Port.
 * 
 * @param config Client configuration (server address, client version)
 * @param username Account username
 * @param password Account password
 * @param mode LoginMode::Login or LoginMode::Register
 * @param session [out] Session to populate with login state
 * @return LoginResponse with result and available worlds
 * 
 * Note: This is a blocking/synchronous call. Use in loading screens.
 */
LoginResponse login(
    const ClientConfig& config,
    const std::string& username,
    const std::string& password,
    req::shared::protocol::LoginMode mode,
    ClientSession& session);

// ============================================================================
// World Handshake
// ============================================================================

enum class CharacterListResult {
    Success,
    ConnectionFailed,
    InvalidSession,
    ProtocolError
};

struct CharacterListResponse {
    CharacterListResult result;
    std::string errorMessage;
    std::vector<req::shared::protocol::CharacterListEntry> characters;
};

/**
 * getCharacterList
 * 
 * Retrieves the list of characters for the current session.
 * Requires valid session.sessionToken and session.worldId from login().
 * 
 * @param session Current session (must have valid sessionToken, worldId)
 * @return CharacterListResponse with character list
 * 
 * Note: This is a blocking/synchronous call.
 */
CharacterListResponse getCharacterList(const ClientSession& session);

struct CharacterCreateResponse {
    CharacterListResult result;  // Same error codes as getCharacterList
    std::string errorMessage;
    req::shared::protocol::CharacterListEntry newCharacter;  // If success
};

/**
 * createCharacter
 * 
 * Creates a new character for the current session.
 * Requires valid session.sessionToken and session.worldId.
 * 
 * @param session Current session
 * @param name Character name
 * @param race Character race (e.g., "Human", "Elf")
 * @param characterClass Character class (e.g., "Warrior", "Wizard")
 * @return CharacterCreateResponse with new character details
 * 
 * Note: This is a blocking/synchronous call.
 */
CharacterCreateResponse createCharacter(
    const ClientSession& session,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass);

enum class EnterWorldResult {
    Success,
    ConnectionFailed,
    InvalidSession,
    InvalidCharacter,
    ProtocolError
};

struct EnterWorldResponse {
    EnterWorldResult result;
    std::string errorMessage;
};

/**
 * enterWorld
 * 
 * Requests to enter the world with a selected character.
 * On success, populates session with handoffToken, zoneId, zoneHost/Port.
 * 
 * @param session [in/out] Current session, updated with zone handoff info
 * @param characterId ID of character to enter world with
 * @return EnterWorldResponse with result
 * 
 * Note: This is a blocking/synchronous call.
 */
EnterWorldResponse enterWorld(
    ClientSession& session,
    std::uint64_t characterId);

// ============================================================================
// Zone Handshake
// ============================================================================

enum class ZoneAuthResult {
    Success,
    ConnectionFailed,
    InvalidHandoff,
    HandoffExpired,
    WrongZone,
    ProtocolError
};

struct ZoneAuthResponse {
    ZoneAuthResult result;
    std::string errorMessage;
    std::string welcomeMessage;  // If success
};

/**
 * connectToZone
 * 
 * Connects to ZoneServer and completes zone authentication.
 * Establishes a persistent connection stored in session.zoneSocket.
 * 
 * @param session [in/out] Current session, updated with zone connection
 * @return ZoneAuthResponse with result and welcome message
 * 
 * Note: This is a blocking call for the initial connection.
 *       After this returns Success, use non-blocking send/receive functions.
 */
ZoneAuthResponse connectToZone(ClientSession& session);

// ============================================================================
// Zone Communication (Non-blocking)
// ============================================================================

/**
 * sendMovementIntent
 * 
 * Sends a MovementIntent message to the zone server.
 * Non-blocking - returns immediately.
 * 
 * @param session Current session (must have active zone connection)
 * @param inputX Movement input X axis (-1.0 to 1.0)
 * @param inputY Movement input Y axis (-1.0 to 1.0)
 * @param facingYaw Facing direction in degrees (0-360)
 * @param jump Jump button state
 * @param sequenceNumber Client sequence number (increment per intent)
 * @return true if message was sent successfully, false otherwise
 */
bool sendMovementIntent(
    const ClientSession& session,
    float inputX, float inputY,
    float facingYaw, bool jump,
    std::uint32_t sequenceNumber);

/**
 * sendAttackRequest
 * 
 * Sends an AttackRequest message to the zone server.
 * Non-blocking - returns immediately.
 * 
 * @param session Current session
 * @param targetId Target entity ID (NPC or player)
 * @param abilityId Ability ID (0 = basic attack)
 * @param isBasicAttack true for basic attack, false for ability
 * @return true if message was sent successfully, false otherwise
 */
bool sendAttackRequest(
    const ClientSession& session,
    std::uint64_t targetId,
    std::uint32_t abilityId = 0,
    bool isBasicAttack = true);

/**
 * sendDevCommand
 * 
 * Sends a DevCommand message to the zone server (admin only).
 * Non-blocking - returns immediately.
 * 
 * @param session Current session (must have isAdmin = true)
 * @param command Command name (e.g., "suicide", "givexp", "setlevel")
 * @param param1 First parameter (optional)
 * @param param2 Second parameter (optional)
 * @return true if message was sent successfully, false otherwise
 */
bool sendDevCommand(
    const ClientSession& session,
    const std::string& command,
    const std::string& param1 = "",
    const std::string& param2 = "");

/**
 * ZoneMessage
 * 
 * Represents a message received from the zone server.
 * Contains raw message type and unparsed payload.
 * Use helper functions to parse specific message types.
 */
struct ZoneMessage {
    req::shared::MessageType type;
    std::string payload;  // Unparsed payload (client parses based on type)
};

/**
 * tryReceiveZoneMessage
 * 
 * Non-blocking receive: Attempts to read a message from the zone server.
 * Returns false if no messages are available (would block).
 * Returns true and fills outMessage if a message was received.
 * 
 * @param session Current session (must have active zone connection)
 * @param outMessage [out] Received message (if return is true)
 * @return true if message received, false if no messages available
 * 
 * Usage: Call this in your main loop to poll for zone messages.
 */
bool tryReceiveZoneMessage(
    const ClientSession& session,
    ZoneMessage& outMessage);

// ============================================================================
// Helper: Parse Common Zone Messages
// ============================================================================

/**
 * parsePlayerStateSnapshot
 * 
 * Helper to parse PlayerStateSnapshot from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed snapshot data
 * @return true if parse succeeded, false otherwise
 */
bool parsePlayerStateSnapshot(
    const std::string& payload,
    req::shared::protocol::PlayerStateSnapshotData& outData);

/**
 * parseAttackResult
 * 
 * Helper to parse AttackResult from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed attack result data
 * @return true if parse succeeded, false otherwise
 */
bool parseAttackResult(
    const std::string& payload,
    req::shared::protocol::AttackResultData& outData);

/**
 * parseDevCommandResponse
 * 
 * Helper to parse DevCommandResponse from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed response data
 * @return true if parse succeeded, false otherwise
 */
bool parseDevCommandResponse(
    const std::string& payload,
    req::shared::protocol::DevCommandResponseData& outData);

/**
 * parseEntitySpawn
 * 
 * Helper to parse EntitySpawn from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed entity spawn data
 * @return true if parse succeeded, false otherwise
 */
bool parseEntitySpawn(
    const std::string& payload,
    req::shared::protocol::EntitySpawnData& outData);

/**
 * parseEntityUpdate
 * 
 * Helper to parse EntityUpdate from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed entity update data
 * @return true if parse succeeded, false otherwise
 */
bool parseEntityUpdate(
    const std::string& payload,
    req::shared::protocol::EntityUpdateData& outData);

/**
 * parseEntityDespawn
 * 
 * Helper to parse EntityDespawn from raw payload.
 * 
 * @param payload Raw payload string from ZoneMessage
 * @param outData [out] Parsed entity despawn data
 * @return true if parse succeeded, false otherwise
 */
bool parseEntityDespawn(
    const std::string& payload,
    req::shared::protocol::EntityDespawnData& outData);

// ============================================================================
// Disconnect / Cleanup
// ============================================================================

/**
 * disconnectFromZone
 * 
 * Gracefully closes the zone connection.
 * Safe to call even if not connected.
 * 
 * @param session [in/out] Session with zone connection to close
 */
void disconnectFromZone(ClientSession& session);

} // namespace req::clientcore
```

---

## 4. Design Rationale

### 4.1 Why Synchronous for Handshake, Non-Blocking for Zone?

**Login/World/Zone handshake:**
- These are **one-shot, sequential operations** that must complete before gameplay starts
- Blocking/synchronous design is **simpler** and matches current TestClient behavior
- UI can show loading screens / spinners during these phases
- Errors are easier to handle (return value, not callbacks)

**Zone communication (movement, combat, snapshots):**
- These are **continuous, high-frequency operations** during gameplay
- Non-blocking (`tryReceiveZoneMessage`) allows client to:
  - Poll for messages in main loop
  - Integrate with SFML event loop or UE tick
  - Avoid freezing the game during network lag
  - Process messages at client's own pace

### 4.2 Why Return Unparsed Payloads in `ZoneMessage`?

Clients have different needs:
- **TestClient:** Wants to parse and log everything for debugging
- **SFML client:** May only care about `PlayerStateSnapshot` for rendering, ignore dev command responses
- **UE client:** May have custom data structures and want to parse directly into them

**Solution:** Core library returns raw `MessageType` + `payload`, provides **optional** parse helpers. Clients choose what to parse.

**Benefits:**
- ? Flexible – clients only parse what they need
- ? Extensible – new message types don't break existing clients
- ? Performance – no unnecessary parsing

### 4.3 Why `ClientSession` Struct Instead of Class with Methods?

**Simplicity:**
- Session is just **data** (tokens, IDs, connection handles)
- All operations are **stateless functions** that take `const ClientSession&`
- No hidden state, no virtual functions, no dynamic dispatch
- Easy to understand and debug

**Testability:**
- Easy to mock/inject session state for unit tests
- No need for dependency injection frameworks
- Can test functions in isolation

**Future-proof:**
- Easy to serialize/deserialize for save games or session recovery
- Can add new fields without breaking API (just add to struct)
- No ABI compatibility issues with adding methods to classes

**Thread-safety:**
- All mutable state is explicit (in the struct)
- Functions don't hold state between calls
- Easy to reason about concurrent access

---

## 5. File Structure (ReqClientCore Static Library)

### 5.1 Proposed Project Layout

```
REQ_Backend/
  REQ_ClientCore/          ? NEW static library
    include/
      req/
        clientcore/
          ClientCore.h     ? Public API (all declarations)
    src/
      ClientCore_Login.cpp     ? Login handshake implementation
      ClientCore_World.cpp     ? World handshake implementation
      ClientCore_Zone.cpp      ? Zone handshake & communication
      ClientCore_Helpers.cpp   ? Message parsing helpers
    REQ_ClientCore.vcxproj   ? Visual Studio project
```

### 5.2 Implementation File Responsibilities

**ClientCore_Login.cpp:**
- `login()` implementation
- Connect to LoginServer
- Send LoginRequest
- Parse LoginResponse
- Populate session with login state

**ClientCore_World.cpp:**
- `getCharacterList()` implementation
- `createCharacter()` implementation
- `enterWorld()` implementation
- Connect to WorldServer
- Handle character management requests

**ClientCore_Zone.cpp:**
- `connectToZone()` implementation
- `sendMovementIntent()` implementation
- `sendAttackRequest()` implementation
- `sendDevCommand()` implementation
- `tryReceiveZoneMessage()` implementation
- `disconnectFromZone()` implementation
- Persistent zone connection management

**ClientCore_Helpers.cpp:**
- `parsePlayerStateSnapshot()` implementation
- `parseAttackResult()` implementation
- `parseDevCommandResponse()` implementation
- `parseEntitySpawn()` implementation
- `parseEntityUpdate()` implementation
- `parseEntityDespawn()` implementation
- Thin wrappers around REQ_Shared protocol parsers

### 5.3 Dependencies

**ReqClientCore depends on:**
- ? **REQ_Shared** (protocol, message types, connection wrapper)
- ? **Boost.Asio** (networking)
- ? Standard C++20 library

**Does NOT depend on:**
- ? REQ_TestClient (no console UI)
- ? SFML
- ? Unreal Engine
- ? Any specific UI framework

**Linking:**
- Static library (`.lib`)
- Clients link against: `REQ_ClientCore.lib`, `REQ_Shared.lib`, Boost libraries

---

## 6. Migration Plan (What Moves, What Stays)

### 6.1 Files to Refactor ? ReqClientCore

| Current File | What to Extract | New Function Name |
|--------------|----------------|-------------------|
| `TestClient.h/cpp` | `doLogin()` | `req::clientcore::login()` |
| `TestClient.h/cpp` | `doCharacterList()` | `req::clientcore::getCharacterList()` |
| `TestClient.h/cpp` | `doCharacterCreate()` | `req::clientcore::createCharacter()` |
| `TestClient.h/cpp` | `doEnterWorld()` | `req::clientcore::enterWorld()` |
| `TestClient_Movement.cpp` | `doZoneAuthAndConnect()` | `req::clientcore::connectToZone()` |
| `TestClient_Movement.cpp` | `SendDevCommand()` | `req::clientcore::sendDevCommand()` |

**Refactoring strategy:**
1. Extract method bodies ? ReqClientCore implementation files
2. Change signatures to use `ClientSession&` instead of `this->`
3. Return result enums + error messages instead of `bool`
4. Remove console logging (`std::cout`) – log via callback or not at all

### 6.2 Files to Keep in TestClient (Console UI)

| File | Purpose | Why It Stays |
|------|---------|--------------|
| `main.cpp` | Menu system, CLI args | Console-specific UI |
| `TestClient_Scenarios_HappyPath.cpp` | Happy path automation | Uses ReqClientCore API, but scenarios stay in test harness |
| `TestClient_Scenarios_Negative.cpp` | Negative test automation | Uses ReqClientCore API for error case testing |
| `BotManager.h/cpp` | Multi-bot orchestration | Test tool, not core functionality |
| `ClientStages.h` | Logging helper | Console-specific stage tracking |

**After refactor:** TestClient becomes a **thin wrapper** around ReqClientCore, adding only:
- Console UI (menus, prompts)
- Test automation (scenarios)
- Logging and debugging output

**Benefits:**
- ? Smaller, more focused TestClient codebase
- ? Core networking tested via TestClient scenarios
- ? SFML/UE clients benefit from tested, stable core

---

## 7. Example Usage Patterns

### 7.1 Console TestClient (After Refactor)

```cpp
#include <req/clientcore/ClientCore.h>
#include <iostream>

int main() {
    using namespace req::clientcore;
    
    ClientConfig config;
    config.loginServerHost = "127.0.0.1";
    config.loginServerPort = 7777;
    
    ClientSession session;
    
    // 1. Login
    std::cout << "Enter username: ";
    std::string username;
    std::getline(std::cin, username);
    
    std::cout << "Enter password: ";
    std::string password;
    std::getline(std::cin, password);
    
    auto loginResp = login(config, username, password, 
                          req::shared::protocol::LoginMode::Login, session);
    
    if (loginResp.result != LoginResult::Success) {
        std::cout << "Login failed: " << loginResp.errorMessage << "\n";
        return 1;
    }
    
    std::cout << "Login successful! Available worlds:\n";
    for (const auto& world : loginResp.availableWorlds) {
        std::cout << "  - " << world.worldName << " (ID: " << world.worldId << ")\n";
    }
    
    // 2. Get character list
    auto charListResp = getCharacterList(session);
    if (charListResp.result != CharacterListResult::Success) {
        std::cout << "Failed to get characters: " << charListResp.errorMessage << "\n";
        return 1;
    }
    
    std::cout << "You have " << charListResp.characters.size() << " character(s)\n";
    
    // 3. Enter world (pick first character or create)
    if (charListResp.characters.empty()) {
        std::cout << "No characters found. Creating default character...\n";
        auto createResp = createCharacter(session, "TestWarrior", "Human", "Warrior");
        if (createResp.result != CharacterListResult::Success) {
            std::cout << "Failed to create character: " << createResp.errorMessage << "\n";
            return 1;
        }
        charListResp.characters.push_back(createResp.newCharacter);
    }
    
    auto enterResp = enterWorld(session, charListResp.characters[0].characterId);
    if (enterResp.result != EnterWorldResult::Success) {
        std::cout << "Failed to enter world: " << enterResp.errorMessage << "\n";
        return 1;
    }
    
    // 4. Connect to zone
    auto zoneResp = connectToZone(session);
    if (zoneResp.result != ZoneAuthResult::Success) {
        std::cout << "Failed to connect to zone: " << zoneResp.errorMessage << "\n";
        return 1;
    }
    
    std::cout << "Connected to zone: " << zoneResp.welcomeMessage << "\n";
    std::cout << "Admin: " << (session.isAdmin ? "YES" : "NO") << "\n";
    
    // 5. Main loop
    std::uint32_t seq = 0;
    while (true) {
        // Get input
        std::cout << "Command (w/a/s/d/attack <id>/q): ";
        std::string cmd;
        std::getline(std::cin, cmd);
        
        if (cmd == "q") break;
        
        // Send movement
        float inputX = 0.0f, inputY = 0.0f;
        if (cmd == "w") inputY = 1.0f;
        if (cmd == "s") inputY = -1.0f;
        if (cmd == "a") inputX = -1.0f;
        if (cmd == "d") inputX = 1.0f;
        
        sendMovementIntent(session, inputX, inputY, 0.0f, false, ++seq);
        
        // Check for zone messages
        ZoneMessage msg;
        while (tryReceiveZoneMessage(session, msg)) {
            if (msg.type == req::shared::MessageType::PlayerStateSnapshot) {
                req::shared::protocol::PlayerStateSnapshotData snapshot;
                if (parsePlayerStateSnapshot(msg.payload, snapshot)) {
                    std::cout << "[Snapshot " << snapshot.snapshotId << "] " 
                             << snapshot.players.size() << " player(s)\n";
                }
            } else if (msg.type == req::shared::MessageType::AttackResult) {
                req::shared::protocol::AttackResultData result;
                if (parseAttackResult(msg.payload, result)) {
                    std::cout << "[Attack] " << result.message << "\n";
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    disconnectFromZone(session);
    return 0;
}
```

### 7.2 SFML Client (REQ_VizTestClient)

```cpp
#include <SFML/Graphics.hpp>
#include <req/clientcore/ClientCore.h>
#include <optional>
#include <unordered_map>

using namespace sf;
using namespace req::clientcore;

int main() {
    RenderWindow window(VideoMode({1280u, 720u}), "REQ VizTestClient");
    window.setFramerateLimit(60);
    
    // Setup client core
    ClientConfig config;
    ClientSession session;
    
    // TODO: Add login UI (username/password input boxes)
    // For now, hardcode test credentials
    auto loginResp = login(config, "testuser", "testpass", 
                          req::shared::protocol::LoginMode::Login, session);
    
    if (loginResp.result != LoginResult::Success) {
        // Show error dialog
        return 1;
    }
    
    // TODO: Add character select UI
    auto charListResp = getCharacterList(session);
    if (charListResp.result != CharacterListResult::Success) {
        return 1;
    }
    
    // Enter world with first character
    if (!charListResp.characters.empty()) {
        auto enterResp = enterWorld(session, charListResp.characters[0].characterId);
        if (enterResp.result != EnterWorldResult::Success) {
            return 1;
        }
    }
    
    // Connect to zone
    auto zoneResp = connectToZone(session);
    if (zoneResp.result != ZoneAuthResult::Success) {
        return 1;
    }
    
    // Player rendering
    std::unordered_map<std::uint64_t, CircleShape> playerShapes;
    
    std::uint32_t movementSeq = 0;
    
    while (window.isOpen()) {
        // Handle SFML events
        while (std::optional event = window.pollEvent()) {
            if (event->is<Event::Closed>()) {
                window.close();
            }
        }
        
        // Send movement based on keyboard input
        float inputX = 0.0f, inputY = 0.0f;
        if (Keyboard::isKeyPressed(Keyboard::Key::W)) inputY = 1.0f;
        if (Keyboard::isKeyPressed(Keyboard::Key::S)) inputY = -1.0f;
        if (Keyboard::isKeyPressed(Keyboard::Key::A)) inputX = -1.0f;
        if (Keyboard::isKeyPressed(Keyboard::Key::D)) inputX = 1.0f;
        
        sendMovementIntent(session, inputX, inputY, 0.0f, false, ++movementSeq);
        
        // Receive zone messages
        ZoneMessage msg;
        while (tryReceiveZoneMessage(session, msg)) {
            if (msg.type == req::shared::MessageType::PlayerStateSnapshot) {
                req::shared::protocol::PlayerStateSnapshotData snapshot;
                if (parsePlayerStateSnapshot(msg.payload, snapshot)) {
                    // Update player positions for rendering
                    for (const auto& player : snapshot.players) {
                        // Create shape if needed
                        if (playerShapes.find(player.characterId) == playerShapes.end()) {
                            CircleShape shape(10.0f);
                            shape.setFillColor(
                                player.characterId == session.selectedCharacterId 
                                ? Color::Green : Color::Red);
                            shape.setOrigin({10.0f, 10.0f});
                            playerShapes[player.characterId] = shape;
                        }
                        
                        // Update position
                        playerShapes[player.characterId].setPosition({player.posX, player.posY});
                    }
                }
            }
        }
        
        // Render
        window.clear(Color(30, 30, 40));
        for (const auto& [id, shape] : playerShapes) {
            window.draw(shape);
        }
        window.display();
    }
    
    disconnectFromZone(session);
    return 0;
}
```

### 7.3 Unreal Engine Client (Future)

```cpp
// In your UE GameInstance or NetworkManager class:

#include <req/clientcore/ClientCore.h>

void AREQGameInstance::LoginToServer(FString Username, FString Password) {
    using namespace req::clientcore;
    
    ClientConfig config;
    // ... setup config from UE settings ...
    
    // Convert FString to std::string
    std::string username = TCHAR_TO_UTF8(*Username);
    std::string password = TCHAR_TO_UTF8(*Password);
    
    // Login (synchronous - show loading screen)
    auto loginResp = login(config, username, password, 
                          req::shared::protocol::LoginMode::Login, ClientSession);
    
    if (loginResp.result != LoginResult::Success) {
        // Show error widget
        OnLoginFailed.Broadcast(FString(loginResp.errorMessage.c_str()));
        return;
    }
    
    OnLoginSuccess.Broadcast();
    // ... proceed to character select ...
}

void AREQGameInstance::Tick(float DeltaTime) {
    using namespace req::clientcore;
    
    // Poll for zone messages (called every UE tick)
    ZoneMessage msg;
    while (tryReceiveZoneMessage(ClientSession, msg)) {
        if (msg.type == req::shared::MessageType::PlayerStateSnapshot) {
            req::shared::protocol::PlayerStateSnapshotData snapshot;
            if (parsePlayerStateSnapshot(msg.payload, snapshot)) {
                // Update UE actors based on snapshot
                for (const auto& player : snapshot.players) {
                    UpdatePlayerActor(player.characterId, 
                                     FVector(player.posX, player.posY, player.posZ));
                }
            }
        }
    }
}
```

---

## 8. Implementation Phases

### Phase 1: Project Setup (Week 1)

**Tasks:**
- ? Create `REQ_ClientCore` static library project in VS solution
- ? Add project references: REQ_Shared, Boost
- ? Create directory structure: `include/req/clientcore/`, `src/`
- ? Create stub `ClientCore.h` with all declarations
- ? Create stub `.cpp` files with empty implementations
- ? Verify project compiles and links

**Deliverable:** Empty ReqClientCore library that compiles

### Phase 2: Login Handshake (Week 2)

**Tasks:**
- ? Implement `login()` in `ClientCore_Login.cpp`
- ? Extract logic from `TestClient::doLogin()`
- ? Convert to use `ClientSession&` instead of class members
- ? Return `LoginResponse` with result enum
- ? Write unit tests (optional)

**Deliverable:** Working `login()` function

### Phase 3: World Handshake (Week 2)

**Tasks:**
- ? Implement `getCharacterList()` in `ClientCore_World.cpp`
- ? Implement `createCharacter()` in `ClientCore_World.cpp`
- ? Implement `enterWorld()` in `ClientCore_World.cpp`
- ? Extract logic from `TestClient` methods
- ? Return appropriate response structs

**Deliverable:** Working world handshake functions

### Phase 4: Zone Handshake & Communication (Week 3)

**Tasks:**
- ? Implement `connectToZone()` in `ClientCore_Zone.cpp`
- ? Implement `sendMovementIntent()`, `sendAttackRequest()`, `sendDevCommand()`
- ? Implement `tryReceiveZoneMessage()`
- ? Implement `disconnectFromZone()`
- ? Extract logic from `TestClient_Movement.cpp`

**Deliverable:** Working zone communication

### Phase 5: Parse Helpers (Week 3)

**Tasks:**
- ? Implement all `parse*()` functions in `ClientCore_Helpers.cpp`
- ? Wrap REQ_Shared protocol parsers
- ? Add error handling

**Deliverable:** Complete parse helper library

### Phase 6: Integrate with TestClient (Week 4)

**Tasks:**
- ? Update TestClient to link against ReqClientCore
- ? Replace `TestClient::doLogin()` calls with `req::clientcore::login()`
- ? Replace all other handshake calls
- ? Update test scenarios to use new API
- ? Remove duplicate code from TestClient
- ? Verify all test scenarios still pass

**Deliverable:** Refactored TestClient using ReqClientCore

### Phase 7: Integrate with SFML Client (Week 5)

**Tasks:**
- ? Update REQ_VizTestClient to link against ReqClientCore
- ? Implement login flow with SFML UI
- ? Implement zone connection
- ? Integrate `tryReceiveZoneMessage()` into SFML event loop
- ? Render players based on `PlayerStateSnapshot`

**Deliverable:** Working SFML client with multiplayer

---

## 9. Testing Strategy

### 9.1 Unit Tests (Optional but Recommended)

**Test framework:** Google Test or Catch2

**Test coverage:**
- ? `login()` with valid/invalid credentials
- ? `getCharacterList()` with valid/invalid session
- ? `createCharacter()` with valid/invalid names
- ? `enterWorld()` with valid/invalid character IDs
- ? `connectToZone()` with valid/invalid handoff tokens
- ? All parse helpers with valid/invalid payloads

**Mock strategy:**
- Mock Boost.Asio sockets for network I/O
- Inject pre-recorded server responses
- Verify correct request payloads

### 9.2 Integration Tests (Via TestClient)

**Use existing TestClient scenarios:**
- ? Happy path scenario (full handshake)
- ? Bad password test
- ? Bad session token test
- ? Bad handoff token test
- ? Negative tests (malformed payloads)

**After Phase 6:** All scenarios should pass using new ReqClientCore API

### 9.3 Manual Testing (Via SFML Client)

**After Phase 7:**
- ? Login with multiple accounts
- ? Create characters
- ? Enter world
- ? Move around with WASD
- ? See other players' positions update
- ? Test disconnect/reconnect

---

## 10. Open Questions for Director

Before implementing, please confirm:

### 10.1 API Surface

**Question:** Is the proposed `ClientCore.h` API minimal enough? Too minimal?

**Options:**
- ? Keep as proposed (minimal, function-based)
- ? Add more high-level abstractions (e.g., `ClientManager` class)
- ? Split into multiple headers (Login.h, World.h, Zone.h)

**Recommendation:** Start minimal, add abstractions later if needed

### 10.2 Synchronous vs Async

**Question:** OK to keep handshake blocking for now? Async can be added later.

**Trade-offs:**
- ? Synchronous: Simpler, matches current TestClient, easier to reason about
- ? Async: More complex, requires callbacks or coroutines, harder to debug

**Recommendation:** Start synchronous, add async overloads later if needed

### 10.3 Error Handling

**Question:** Return enum + string message, or use exceptions?

**Current design:** Return result enum + error message string

**Alternative:** Throw exceptions for errors

**Recommendation:** Keep enum-based for predictable control flow

### 10.4 Session Lifetime

**Question:** Should `ClientSession` be a class with RAII cleanup, or keep as plain struct?

**Current design:** Plain struct, cleanup via `disconnectFromZone()`

**Alternative:** Class with destructor that auto-disconnects

**Recommendation:** Keep as struct for now, add RAII wrapper if needed

### 10.5 Logging

**Question:** Should core library log directly, or provide logging callbacks?

**Options:**
- Log directly using REQ_Shared logger (simplest)
- Provide optional logging callback (most flexible)
- No logging at all (let clients handle)

**Recommendation:** Use REQ_Shared logger for now, add callbacks later if needed

---

## 11. Benefits of This Design

### 11.1 Code Reuse

? **Single source of truth** for client networking
- No duplicated handshake code between console/SFML/UE clients
- Bug fixes propagate to all clients automatically
- Protocol changes only require updating one library

### 11.2 Maintainability

? **Clean separation of concerns:**
- Core networking: ReqClientCore
- Console UI: TestClient
- SFML rendering: REQ_VizTestClient
- UE rendering: Future UE client

? **Easier to understand:**
- Each project has a clear, focused purpose
- No mixing of UI and networking code

### 11.3 Testability

? **Core logic tested independently:**
- Unit tests for ReqClientCore (no UI dependencies)
- Integration tests via TestClient scenarios
- Manual tests via SFML/UE clients

? **Mock-friendly:**
- Stateless functions easy to mock
- Session is just data (easy to inject test state)

### 11.4 Extensibility

? **Easy to add new clients:**
- Qt client? Just link ReqClientCore
- Web client (via Emscripten)? Reuse same API
- Mobile client? Port core library

? **Easy to add new features:**
- Add function to ReqClientCore
- All clients get it automatically
- No need to update each client separately

### 11.5 Performance

? **Zero overhead abstraction:**
- Inline functions for hot paths
- No virtual dispatch
- No heap allocations in message send/receive

? **Non-blocking zone communication:**
- Clients control when to poll messages
- No forced threading model

---

## 12. Risks and Mitigations

### 12.1 Risk: Boost.Asio Version Incompatibilities

**Risk:** Different Boost versions between projects

**Mitigation:**
- Pin Boost version in documentation
- Use common Boost install for all projects
- Test with CI/CD to catch version issues early

### 12.2 Risk: API Instability During Development

**Risk:** API changes break existing code

**Mitigation:**
- Start with internal API (not public release)
- Version the library (`clientcore_v1`, `clientcore_v2`)
- Deprecate old APIs before removing

### 12.3 Risk: Platform Portability

**Risk:** Boost.Asio may have platform-specific quirks

**Mitigation:**
- Test on Windows + Linux early
- Use Boost abstractions (don't call OS APIs directly)
- Add platform-specific workarounds if needed

### 12.4 Risk: Complex Async Patterns Later

**Risk:** Synchronous API may not fit async UE needs

**Mitigation:**
- UE can run synchronous calls on background threads
- Add async overloads later if needed (e.g., `loginAsync()`)
- Keep synchronous API as "simple mode"

---

## 13. Success Criteria

### 13.1 Phase 6 Success (TestClient Refactor)

? TestClient compiles and links against ReqClientCore  
? All existing test scenarios pass  
? No duplicated handshake code in TestClient  
? TestClient is <50% original size (mostly UI now)

### 13.2 Phase 7 Success (SFML Integration)

? REQ_VizTestClient compiles and links against ReqClientCore  
? Can login, enter world, connect to zone  
? WASD movement sends `MovementIntent`  
? Receives and renders `PlayerStateSnapshot`  
? Multiple SFML clients can see each other move

### 13.3 Overall Success

? Zero code duplication between clients  
? Core networking is stable and well-tested  
? Easy for new developers to add clients  
? Documentation is clear and complete

---

## 14. Documentation Plan

### 14.1 API Documentation

**Format:** Doxygen comments in `ClientCore.h`

**Content:**
- Function purpose and usage
- Parameter descriptions
- Return value meanings
- Example code snippets
- Error handling guidance

### 14.2 Integration Guides

**Console Client Guide:**
- How to use ReqClientCore in console app
- Example: TestClient refactor

**SFML Client Guide:**
- How to integrate with SFML event loop
- Example: REQ_VizTestClient

**UE Client Guide (Future):**
- How to integrate with UE tick
- How to convert between UE and REQ types

### 14.3 Protocol Documentation

**Reference existing docs:**
- Link to `docs/REQ_PROTOCOL_*.md`
- Explain how ReqClientCore implements protocol

---

## 15. Appendix: Full File Listing

### 15.1 ReqClientCore Files (After Implementation)

```
REQ_ClientCore/
??? include/
?   ??? req/
?       ??? clientcore/
?           ??? ClientCore.h              (Public API, ~600 lines)
??? src/
?   ??? ClientCore_Login.cpp             (~200 lines)
?   ??? ClientCore_World.cpp             (~300 lines)
?   ??? ClientCore_Zone.cpp              (~400 lines)
?   ??? ClientCore_Helpers.cpp           (~100 lines)
??? REQ_ClientCore.vcxproj
```

**Total:** ~1600 lines of implementation code

**Compared to current TestClient:**
- TestClient.cpp: ~500 lines (mostly handshake)
- TestClient_Movement.cpp: ~400 lines (mostly zone communication)
- **Total extracted:** ~900 lines

**Remaining in TestClient after refactor:**
- main.cpp: ~200 lines (menu system)
- TestClient_Scenarios*.cpp: ~600 lines (test automation)
- **New TestClient size:** ~800 lines (47% reduction)

### 15.2 TestClient Files (After Refactor)

```
REQ_TestClient/
??? include/
?   ??? req/
?       ??? testclient/
?           ??? TestClient.h             (Thin wrapper around ClientCore)
?           ??? BotClient.h              (Uses ClientCore API)
?           ??? BotManager.h
?           ??? ClientStages.h
??? src/
?   ??? main.cpp                         (Menu system, CLI args)
?   ??? TestClient.cpp                   (Calls ClientCore functions)
?   ??? TestClient_Scenarios_HappyPath.cpp
?   ??? TestClient_Scenarios_Negative.cpp
?   ??? BotClient.cpp                    (Uses ClientCore API)
?   ??? BotManager.cpp
??? REQ_TestClient.vcxproj
```

---

## 16. Timeline Summary

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| 1. Project Setup | 1 week | Empty ReqClientCore library |
| 2. Login Handshake | 1 week | `login()` function |
| 3. World Handshake | 1 week | Character management functions |
| 4. Zone Communication | 1 week | Zone send/receive functions |
| 5. Parse Helpers | 1 week | All parse helper functions |
| 6. TestClient Integration | 1 week | Refactored TestClient |
| 7. SFML Integration | 1 week | Working multiplayer SFML client |

**Total:** ~7 weeks (aggressive but achievable)

**Can be parallelized:**
- Phases 2-5 (implementation) can overlap if multiple developers
- Phase 6-7 (integration) should be sequential

---

## 17. Conclusion

### 17.1 Summary

ReqClientCore provides a **clean, minimal API** for REQ client networking:

? **Stateless functions** – easy to understand and test  
? **Synchronous handshake** – simple, blocking, sequential  
? **Non-blocking zone communication** – integrate with any event loop  
? **Shared by all clients** – no code duplication  
? **Extensible** – easy to add new functions/features

### 17.2 Next Steps

**Immediate:**
1. Review this design document
2. Answer open questions (Section 10)
3. Approve or request changes

**After approval:**
1. Create REQ_ClientCore project (Phase 1)
2. Extract Login handshake (Phase 2)
3. Continue through phases 3-7

**Long-term:**
- Add async overloads if needed
- Add logging callbacks if needed
- Add more helper functions as clients request them
- Port to other platforms (Linux, macOS, mobile)

---

**Status:** ? **Design complete – awaiting director approval**

**Document Version:** 1.0  
**Last Updated:** 2024

---

**End of Design Document**
