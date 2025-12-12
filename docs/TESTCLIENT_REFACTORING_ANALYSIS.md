# TestClient Networking Entry Points Analysis

## Current State (Before Refactoring)

### Main Networking Functions

#### TestClient Class (`REQ_TestClient/src/TestClient.cpp`)

**Login Flow:**
- `bool doLogin(username, password, clientVersion, mode, ...)`
  - Creates `boost::asio::io_context`
  - Creates `Tcp::socket`
  - Connects to LoginServer (127.0.0.1:7777)
  - Builds `LoginRequest` payload
  - Sends/receives via custom `sendMessage`/`receiveMessage` helpers
  - Parses `LoginResponse`
  - Returns session token, world info

**Character Management:**
- `bool doCharacterList(worldHost, worldPort, sessionToken, worldId, ...)`
  - Creates new socket per call
  - Connects to WorldServer
  - Sends `CharacterListRequest`
  - Parses `CharacterListResponse`

- `bool doCharacterCreate(worldHost, worldPort, sessionToken, worldId, name, race, class, ...)`
  - Creates new socket per call
  - Connects to WorldServer
  - Sends `CharacterCreateRequest`
  - Parses `CharacterCreateResponse`

- `bool doEnterWorld(worldHost, worldPort, sessionToken, worldId, characterId, ...)`
  - Creates new socket per call
  - Connects to WorldServer
  - Sends `EnterWorldRequest`
  - Parses `EnterWorldResponse`
  - Returns handoff token, zone info

**Zone Connection:**
- `bool doZoneAuthAndConnect(zoneHost, zonePort, handoffToken, characterId, ...)`
  - Creates **persistent** `io_context` and `socket` (stored in shared_ptr)
  - Connects to ZoneServer
  - Sends `ZoneAuthRequest`
  - Parses `ZoneAuthResponse`
  - Returns persistent socket for movement loop

**Movement/Commands:**
- `void runMovementTestLoop(ioContext, zoneSocket, characterId)`
  - Interactive loop with user commands
  - Sends `MovementIntent` messages
  - Sends `AttackRequest` messages
  - Sends `DevCommand` messages (if admin)
  - Polls zone messages via `tryReceiveMessage`
  - Parses zone snapshots and events

### Helper Functions (Anonymous Namespace)

- `bool sendMessage(socket, type, body)`
  - Builds `MessageHeader`
  - Writes header + body to socket
  
- `bool receiveMessage(socket, header, body)`
  - Reads header from socket
  - Reads body from socket
  - Returns parsed header + body string

- `bool tryReceiveMessage(socket, header, body)` (in Movement file)
  - Non-blocking read attempt
  - Returns `false` if no data available

### Session State Storage

Current TestClient stores state in member variables:
```cpp
req::shared::SessionToken sessionToken_;
std::uint64_t accountId_;
req::shared::WorldId worldId_;
req::shared::HandoffToken handoffToken_;
req::shared::ZoneId zoneId_;
std::uint64_t selectedCharacterId_;
bool isAdmin_;
```

Plus temporary variables in `run()`:
```cpp
std::string worldHost;
std::uint16_t worldPort;
std::string zoneHost;
std::uint16_t zonePort;
std::shared_ptr<boost::asio::io_context> zoneIoContext;
std::shared_ptr<Tcp::socket> zoneSocket;
```

---

## BotClient Class (`REQ_TestClient/src/BotClient.cpp`)

BotClient has the **exact same networking pattern** as TestClient:
- `bool doLogin()` - Login with credentials
- `bool doCharacterList()` - Get/create character
- `bool doCharacterCreate()` - Create character if needed
- `bool doEnterWorld()` - Get zone handoff
- `bool doZoneAuth()` - Connect to zone
- `void tick()` - Movement and message polling

BotClient also stores persistent zone socket and uses same `sendMessage`/`receiveMessage` helpers.

---

## Mapping to REQ_ClientCore API

### REQ_ClientCore provides:

**Configuration:**
```cpp
ClientConfig config{};
config.clientVersion = "...";
config.loginServerHost = "127.0.0.1";
config.loginServerPort = 7777;
```

**Session State (replaces all member variables):**
```cpp
ClientSession session{};
// Contains: sessionToken, worldId, worldHost, worldPort,
//           handoffToken, zoneId, zoneHost, zonePort,
//           selectedCharacterId, zoneSocket, etc.
```

**Login:**
```cpp
LoginResponse loginResp = login(config, username, password, mode, session);
// session now populated with sessionToken, worldId, worldHost, worldPort
```

**Character Management:**
```cpp
CharacterListResponse charListResp = getCharacterList(session);
CharacterCreateResponse createResp = createCharacter(session, name, race, classId);
EnterWorldResponse enterResp = enterWorld(session, characterId);
// session now populated with handoffToken, zoneId, zoneHost, zonePort
```

**Zone Connection:**
```cpp
ZoneAuthResponse zoneResp = connectToZone(session);
// session.zoneSocket is now persistent connection
```

**Zone Messages:**
```cpp
// Non-blocking send (replaces sendMessage for zone)
sendMovementIntent(session, inputX, inputY, yaw, jump, seq);
sendAttackRequest(session, targetId, abilityId, isBasicAttack);
sendDevCommand(session, command, param1, param2);

// Non-blocking receive (replaces tryReceiveMessage)
ZoneMessage msg;
while (tryReceiveZoneMessage(session, msg)) {
    // Dispatch based on msg.type
    if (msg.type == MessageType::PlayerStateSnapshot) {
        PlayerStateSnapshotData snapshot;
        parsePlayerStateSnapshot(msg.payload, snapshot);
        // Handle snapshot...
    }
}
```

---

## Refactoring Strategy

### Phase 1: TestClient Class

#### Step 1: Update TestClient.h
- Remove `using Tcp` and `using IoContext`
- Remove session state member variables (use `ClientSession` instead)
- Add `ClientSession session_` member
- Add `ClientConfig config_` member
- Remove `doLogin`, `doCharacterList`, `doCharacterCreate`, `doEnterWorld`, `doZoneAuthAndConnect`
- Simplify `runMovementTestLoop` to use `ClientSession`

#### Step 2: Update TestClient.cpp
- Remove anonymous namespace helpers (`sendMessage`, `receiveMessage`)
- Update `run()` to use ClientCore API:
  ```cpp
  void TestClient::run() {
      // Prompt for username/password
      auto loginResp = login(config_, username, password, mode, session_);
      if (loginResp.result != LoginResult::Success) { /* error */ }
      
      auto charListResp = getCharacterList(session_);
      if (charListResp.result != CharacterListResult::Success) { /* error */ }
      
      // Create character if needed
      if (charListResp.characters.empty()) {
          auto createResp = createCharacter(session_, name, race, classId);
      }
      
      auto enterResp = enterWorld(session_, characterId);
      if (enterResp.result != EnterWorldResult::Success) { /* error */ }
      
      auto zoneResp = connectToZone(session_);
      if (zoneResp.result != ZoneAuthResult::Success) { /* error */ }
      
      runMovementTestLoop();  // Uses session_ internally
  }
  ```

#### Step 3: Update TestClient_Movement.cpp
- Replace `tryReceiveMessage` with `tryReceiveZoneMessage`
- Replace `sendMessage` with `sendMovementIntent`, `sendAttackRequest`, `sendDevCommand`
- Use `session_` instead of passing socket

#### Step 4: Update TestClient_Scenarios.cpp
- Update all scenario methods to use ClientCore API
- Remove socket creation/management
- Use `session_` for all operations

### Phase 2: BotClient Class

Apply same changes as TestClient:
- Add `ClientSession` and `ClientConfig` members
- Remove socket management
- Update all `do*` methods to use ClientCore API
- Simplify `tick()` to use `tryReceiveZoneMessage`

### Phase 3: Cleanup

#### Remove Direct Boost.Asio Usage:
- Remove `#include <boost/asio.hpp>` from TestClient/BotClient headers
- Remove `using Tcp = boost::asio::ip::tcp;`
- Remove `using IoContext = boost::asio::io_context;`
- Keep Boost.Asio include in ClientCore (where it belongs)

#### Files That Can Be Simplified:
- `TestClient.h` - Much simpler interface
- `TestClient.cpp` - No socket management
- `TestClient_Movement.cpp` - Simpler message handling
- `BotClient.h` - Simpler interface
- `BotClient.cpp` - No socket management

---

## Expected Benefits

### Code Reduction:
- ~300 lines of socket management code removed
- ~100 lines of message serialization removed
- All handshake logic delegated to ClientCore

### Maintainability:
- Single source of truth for client networking (ClientCore)
- TestClient focuses on UI/flow, not networking
- BotClient focuses on AI behavior, not networking

### Testability:
- ClientCore can be unit-tested independently
- TestClient can be tested without mocking Boost.Asio

### Future Flexibility:
- ClientCore can add connection pooling
- ClientCore can add automatic reconnection
- ClientCore can switch transport (e.g., WebSockets) without changing TestClient

---

## Files to Modify

### TestClient:
- `REQ_TestClient/include/req/testclient/TestClient.h`
- `REQ_TestClient/src/TestClient.cpp`
- `REQ_TestClient/src/TestClient_Movement.cpp`
- `REQ_TestClient/src/TestClient_Scenarios.cpp`
- `REQ_TestClient/src/TestClient_Scenarios_HappyPath.cpp`
- `REQ_TestClient/src/TestClient_Scenarios_Negative.cpp`
- `REQ_TestClient/src/TestClient_Characters.cpp`
- `REQ_TestClient/src/TestClient_World.cpp`

### BotClient:
- `REQ_TestClient/include/req/testclient/BotClient.h`
- `REQ_TestClient/src/BotClient.cpp`
- `REQ_TestClient/src/BotClient_Movement.cpp`

### No Changes Needed:
- `REQ_TestClient/src/main.cpp` - Already uses TestClient/BotClient abstractions
- `REQ_TestClient/src/BotManager.cpp` - Manages BotClient instances

---

## Verification Checklist

After refactoring:
- [ ] Build succeeds with no errors
- [ ] Happy path scenario works
- [ ] Interactive mode works
- [ ] Bot mode works
- [ ] All commands functional (movement, attack, dev)
- [ ] No Boost.Asio includes in TestClient/BotClient headers
- [ ] All session state stored in `ClientSession`
- [ ] Zone socket properly managed by ClientCore

---

## Summary

**Current State:**
- TestClient manually manages sockets, connections, and serialization
- ~800 lines of networking code in TestClient
- Direct Boost.Asio usage throughout

**Target State:**
- TestClient uses REQ_ClientCore API exclusively
- ~500 lines of networking code removed
- Clean separation: ClientCore handles networking, TestClient handles UI/flow
- BotClient also benefits from same simplification

**Next Steps:**
1. Start with TestClient.h refactor
2. Update TestClient.cpp
3. Update Movement/Scenarios files
4. Apply same to BotClient
5. Build and test

**Estimated LOC Change:**
- Lines removed: ~500
- Lines added: ~100 (ClientCore API calls)
- Net reduction: ~400 lines
