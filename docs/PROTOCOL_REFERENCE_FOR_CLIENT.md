# REQ Backend Protocol Reference for Client Implementation

This document contains all the protocol definitions, data structures, and message formats needed to implement a client for the REQ Backend server.

---

## Table of Contents

1. [Message Types](#message-types)
2. [Core Type Definitions](#core-type-definitions)
3. [Authentication Flow](#authentication-flow)
4. [Character Management](#character-management)
5. [Movement Protocol](#movement-protocol)
6. [Helper Functions](#helper-functions)

---

## Message Types

All messages use these type identifiers in the `MessageHeader`:

```cpp
enum class MessageType : std::uint16_t {
    // Generic / debug
    Ping = 0,   // Client or server ping
    Pong = 1,   // Response to Ping

    // Login server handshake/auth
    LoginRequest      = 10, // Client requests login with credentials
    LoginResponse     = 11, // Server responds with success/fail + token

    // World server authentication / selection
    WorldAuthRequest  = 20, // Client presents session to world server
    WorldAuthResponse = 21, // World server validates session
    
    // Character management
    CharacterListRequest   = 22, // Client requests character list for account/world
    CharacterListResponse  = 23, // World server responds with character list
    CharacterCreateRequest = 24, // Client requests character creation
    CharacterCreateResponse = 25, // World server responds with created character
    EnterWorldRequest      = 26, // Client requests to enter world with character
    EnterWorldResponse     = 27, // World server responds with zone handoff

    // Zone server handoff/authentication
    ZoneAuthRequest   = 30, // Client requests entry to zone with handoff token
    ZoneAuthResponse  = 31, // Zone server confirms access
    
    // Zone gameplay - Movement (server-authoritative model)
    MovementIntent        = 40, // Client sends movement input to ZoneServer
    PlayerStateSnapshot   = 41, // ZoneServer sends authoritative player states to client

    // Gameplay (initial placeholders)
    PlayerState       = 100, // DEPRECATED - use PlayerStateSnapshot
    NpcSpawn          = 101, // NPC spawn info (TODO payload format)
    ChatMessage       = 102, // Chat channel or direct message (TODO payload format)
};
```

---

## Core Type Definitions

These types are used throughout the protocol (from `Types.h`):

```cpp
// Session and authentication tokens
using SessionToken = std::uint64_t;
using HandoffToken = std::uint64_t;

constexpr SessionToken InvalidSessionToken = 0;
constexpr HandoffToken InvalidHandoffToken = 0;

// World and zone identifiers
using WorldId = std::uint32_t;
using ZoneId = std::uint32_t;
using PlayerId = std::uint64_t;

// Protocol version
constexpr std::uint16_t CurrentProtocolVersion = 1;
```

---

## Authentication Flow

### 1. Login (Client ? LoginServer)

#### LoginRequest

**Message Type:** `MessageType::LoginRequest` (10)

**Payload Format:**
```
username|password|clientVersion|mode
```

**Fields:**
- `username`: string - Player account username
- `password`: string - Player account password (plaintext for now)
- `clientVersion`: string - Client version for compatibility checks
- `mode`: string - "login" or "register" (defaults to "login" if omitted)

**Example:**
```
player1|mypassword|0.1.0|login
newuser|newpass|0.1.0|register
```

**Helper Functions:**
```cpp
std::string buildLoginRequestPayload(
    const std::string& username,
    const std::string& password,
    const std::string& clientVersion,
    LoginMode mode = LoginMode::Login);
```

---

#### LoginResponse

**Message Type:** `MessageType::LoginResponse` (11)

**Success Format:**
```
OK|sessionToken|worldCount|world1Data|world2Data|...
```

**World Data Format (comma-separated):**
```
worldId,worldName,worldHost,worldPort,rulesetId
```

**Error Format:**
```
ERR|errorCode|errorMessage
```

**Example Success:**
```
OK|123456789|2|1,MainWorld,127.0.0.1,7778,standard|2,TestWorld,127.0.0.1,7779,pvp
```

**Example Error:**
```
ERR|AUTH_FAILED|Invalid username or password
```

**Data Structure:**
```cpp
enum class LoginMode {
    Login,
    Register
};

struct WorldListEntry {
    WorldId worldId{ 0 };
    std::string worldName;
    std::string worldHost;
    std::uint16_t worldPort{ 0 };
    std::string rulesetId;
};

struct LoginResponseData {
    bool success{ false };
    
    // Success fields
    SessionToken sessionToken{ InvalidSessionToken };
    std::vector<WorldListEntry> worlds;
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};
```

**Helper Functions:**
```cpp
bool parseLoginResponsePayload(
    const std::string& payload,
    LoginResponseData& outData);
```

---

### 2. World Authentication (Client ? WorldServer)

#### WorldAuthRequest

**Message Type:** `MessageType::WorldAuthRequest` (20)

**Payload Format:**
```
sessionToken|worldId
```

**Example:**
```
123456789|1
```

---

#### WorldAuthResponse

**Message Type:** `MessageType::WorldAuthResponse` (21)

**Success Format:**
```
OK|handoffToken|zoneId|zoneHost|zonePort
```

**Error Format:**
```
ERR|errorCode|errorMessage
```

**Data Structure:**
```cpp
struct WorldAuthResponseData {
    bool success{ false };
    
    // Success fields
    HandoffToken handoffToken{ InvalidHandoffToken };
    ZoneId zoneId{ 0 };
    std::string zoneHost;
    std::uint16_t zonePort{ 0 };
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};
```

---

### 3. Zone Authentication (Client ? ZoneServer)

#### ZoneAuthRequest

**Message Type:** `MessageType::ZoneAuthRequest` (30)

**Payload Format:**
```
handoffToken|characterId
```

**Example:**
```
987654321|42
```

**Helper Functions:**
```cpp
std::string buildZoneAuthRequestPayload(
    HandoffToken handoffToken,
    PlayerId characterId);
```

---

#### ZoneAuthResponse

**Message Type:** `MessageType::ZoneAuthResponse` (31)

**Success Format:**
```
OK|welcomeMessage
```

**Error Format:**
```
ERR|errorCode|errorMessage
```

**Data Structure:**
```cpp
struct ZoneAuthResponseData {
    bool success{ false };
    
    // Success fields
    std::string welcomeMessage;
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};
```

**Helper Functions:**
```cpp
bool parseZoneAuthResponsePayload(
    const std::string& payload,
    ZoneAuthResponseData& outData);
```

---

## Character Management

### CharacterListRequest

**Message Type:** `MessageType::CharacterListRequest` (22)

**Payload Format:**
```
sessionToken|worldId
```

**Example:**
```
123456789|1
```

**Helper Functions:**
```cpp
std::string buildCharacterListRequestPayload(
    SessionToken sessionToken,
    WorldId worldId);
```

---

### CharacterListResponse

**Message Type:** `MessageType::CharacterListResponse` (23)

**Success Format:**
```
OK|characterCount|char1Data|char2Data|...
```

**Character Data Format (comma-separated):**
```
characterId,name,race,class,level
```

**Example:**
```
OK|2|1,Arthas,Human,Paladin,5|2,Thrall,Orc,Shaman,3
```

**Data Structure:**
```cpp
struct CharacterListEntry {
    std::uint64_t characterId{ 0 };
    std::string name;
    std::string race;
    std::string characterClass;
    std::uint32_t level{ 1 };
};

struct CharacterListResponseData {
    bool success{ false };
    
    // Success fields
    std::vector<CharacterListEntry> characters;
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};
```

**Helper Functions:**
```cpp
bool parseCharacterListResponsePayload(
    const std::string& payload,
    CharacterListResponseData& outData);
```

---

### CharacterCreateRequest

**Message Type:** `MessageType::CharacterCreateRequest` (24)

**Payload Format:**
```
sessionToken|worldId|name|race|class
```

**Example:**
```
123456789|1|Arthas|Human|Paladin
```

**Helper Functions:**
```cpp
std::string buildCharacterCreateRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass);
```

---

### CharacterCreateResponse

**Message Type:** `MessageType::CharacterCreateResponse` (25)

**Success Format:**
```
OK|characterId|name|race|class|level
```

**Example:**
```
OK|42|Arthas|Human|Paladin|1
```

**Data Structure:**
```cpp
struct CharacterCreateResponseData {
    bool success{ false };
    
    // Success fields
    std::uint64_t characterId{ 0 };
    std::string name;
    std::string race;
    std::string characterClass;
    std::uint32_t level{ 1 };
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};
```

**Helper Functions:**
```cpp
bool parseCharacterCreateResponsePayload(
    const std::string& payload,
    CharacterCreateResponseData& outData);
```

---

### EnterWorldRequest

**Message Type:** `MessageType::EnterWorldRequest` (26)

**Payload Format:**
```
sessionToken|worldId|characterId
```

**Example:**
```
123456789|1|42
```

**Helper Functions:**
```cpp
std::string buildEnterWorldRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    std::uint64_t characterId);
```

---

### EnterWorldResponse

**Message Type:** `MessageType::EnterWorldResponse` (27)

**Success Format:**
```
OK|handoffToken|zoneId|zoneHost|zonePort
```

**Example:**
```
OK|987654321|10|127.0.0.1|7780
```

**Data Structure:**
```cpp
struct EnterWorldResponseData {
    bool success{ false };
    
    // Success fields
    HandoffToken handoffToken{ InvalidHandoffToken };
    ZoneId zoneId{ 0 };
    std::string zoneHost;
    std::uint16_t zonePort{ 0 };
    
    // Error fields
    std::string errorCode;
    std::string errorMessage;
};
```

**Helper Functions:**
```cpp
bool parseEnterWorldResponsePayload(
    const std::string& payload,
    EnterWorldResponseData& outData);
```

---

## Movement Protocol

**IMPORTANT:** This implements a **server-authoritative movement model**:
- Client sends ONLY input (not position)
- Server computes all positions and velocities
- Client receives authoritative state from server
- Client should predict locally but accept server corrections

---

### MovementIntent (Client ? ZoneServer)

**Message Type:** `MessageType::MovementIntent` (40)

**Direction:** Client ? ZoneServer

**Payload Format:**
```
characterId|sequenceNumber|inputX|inputY|facingYawDegrees|isJumpPressed|clientTimeMs
```

**Fields:**
- `characterId`: uint64 - Character sending the input
- `sequenceNumber`: uint32 - Increments per intent from this client (for packet ordering)
- `inputX`: float - Movement input X axis: -1.0 to 1.0 (left/right, strafe)
- `inputY`: float - Movement input Y axis: -1.0 to 1.0 (forward/backward)
- `facingYawDegrees`: float - Facing direction: 0-360 degrees (server normalizes)
- `isJumpPressed`: int - 0 or 1 (jump button state)
- `clientTimeMs`: uint32 - Client timestamp in milliseconds (for debugging/telemetry)

**Example:**
```
42|123|0.5|-1.0|90.0|1|1234567890
```

This example shows:
- Character ID: 42
- Sequence: 123
- Input: moving right (0.5) and backward (-1.0)
- Facing: 90 degrees (East)
- Jumping: yes (1)
- Client time: 1234567890 ms

**Data Structure:**
```cpp
struct MovementIntentData {
    std::uint64_t characterId{ 0 };      // Character sending the input
    std::uint32_t sequenceNumber{ 0 };   // Increments per intent from this client
    float inputX{ 0.0f };                // Movement input X axis: -1.0 to 1.0
    float inputY{ 0.0f };                // Movement input Y axis: -1.0 to 1.0
    float facingYawDegrees{ 0.0f };      // Facing direction: 0-360 degrees
    bool isJumpPressed{ false };         // Jump button state
    std::uint32_t clientTimeMs{ 0 };     // Client timestamp (for debugging/telemetry)
};
```

**Helper Functions:**
```cpp
std::string buildMovementIntentPayload(
    const MovementIntentData& data);

bool parseMovementIntentPayload(
    const std::string& payload,
    MovementIntentData& outData);
```

**Usage (Client):**
```cpp
// Build movement intent from user input
MovementIntentData intent;
intent.characterId = myCharacterId;
intent.sequenceNumber = nextSequence++;
intent.inputX = getHorizontalInput();      // -1 to 1 from keyboard/gamepad
intent.inputY = getVerticalInput();        // -1 to 1 from keyboard/gamepad
intent.facingYawDegrees = getCameraYaw();  // 0-360
intent.isJumpPressed = isJumpKeyDown();
intent.clientTimeMs = getCurrentTimeMs();

// Build payload and send
std::string payload = buildMovementIntentPayload(intent);
sendMessage(MessageType::MovementIntent, payload);
```

---

### PlayerStateSnapshot (ZoneServer ? Client)

**Message Type:** `MessageType::PlayerStateSnapshot` (41)

**Direction:** ZoneServer ? Client

**Frequency:** Typically 20 Hz (every 50ms)

**Payload Format:**
```
snapshotId|playerCount|player1Data|player2Data|...
```

**Player Data Format (comma-separated):**
```
characterId,posX,posY,posZ,velX,velY,velZ,yawDegrees
```

**Fields:**
- `snapshotId`: uint64 - Incrementing snapshot identifier
- `playerCount`: uint64 - Number of players in this snapshot
- Per Player:
  - `characterId`: uint64 - Character ID
  - `posX`, `posY`, `posZ`: float - World position (X, Y, Z coordinates)
  - `velX`, `velY`, `velZ`: float - Velocity (units/second)
  - `yawDegrees`: float - Facing direction (0-360 degrees)

**Example:**
```
5|2|42,100.5,200.0,10.0,0.0,0.0,0.0,90.0|43,150.0,200.0,10.0,1.5,0.0,0.0,180.0
```

This example shows:
- Snapshot ID: 5
- 2 players
- Player 1: Character 42 at (100.5, 200.0, 10.0), stationary, facing 90°
- Player 2: Character 43 at (150.0, 200.0, 10.0), velocity (1.5, 0, 0), facing 180°

**Data Structures:**
```cpp
struct PlayerStateEntry {
    std::uint64_t characterId{ 0 };
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    float velX{ 0.0f };
    float velY{ 0.0f };
    float velZ{ 0.0f };
    float yawDegrees{ 0.0f };
};

struct PlayerStateSnapshotData {
    std::uint64_t snapshotId{ 0 };                    // Incrementing snapshot identifier
    std::vector<PlayerStateEntry> players;             // All players in this snapshot
};
```

**Helper Functions:**
```cpp
std::string buildPlayerStateSnapshotPayload(
    const PlayerStateSnapshotData& data);

bool parsePlayerStateSnapshotPayload(
    const std::string& payload,
    PlayerStateSnapshotData& outData);
```

**Usage (Client):**
```cpp
// Receive and parse snapshot
PlayerStateSnapshotData snapshot;
if (parsePlayerStateSnapshotPayload(payload, snapshot)) {
    for (const auto& player : snapshot.players) {
        if (player.characterId == myCharacterId) {
            // Apply server correction to local player
            applyServerCorrection(player.posX, player.posY, player.posZ);
            updateVelocity(player.velX, player.velY, player.velZ);
            updateYaw(player.yawDegrees);
        } else {
            // Update other players
            updateRemotePlayer(player);
        }
    }
}
```

---

## Helper Functions

All helper functions are in the `req::shared::protocol` namespace.

### Build Functions

These create payload strings from data structures:

```cpp
// Authentication
std::string buildLoginRequestPayload(
    const std::string& username,
    const std::string& password,
    const std::string& clientVersion,
    LoginMode mode = LoginMode::Login);

std::string buildWorldAuthRequestPayload(
    SessionToken sessionToken,
    WorldId worldId);

std::string buildZoneAuthRequestPayload(
    HandoffToken handoffToken,
    PlayerId characterId);

// Character Management
std::string buildCharacterListRequestPayload(
    SessionToken sessionToken,
    WorldId worldId);

std::string buildCharacterCreateRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    const std::string& name,
    const std::string& race,
    const std::string& characterClass);

std::string buildEnterWorldRequestPayload(
    SessionToken sessionToken,
    WorldId worldId,
    std::uint64_t characterId);

// Movement
std::string buildMovementIntentPayload(
    const MovementIntentData& data);
```

### Parse Functions

These parse payload strings into data structures:

```cpp
// Authentication
bool parseLoginRequestPayload(
    const std::string& payload,
    std::string& outUsername,
    std::string& outPassword,
    std::string& outClientVersion,
    LoginMode& outMode);

bool parseLoginResponsePayload(
    const std::string& payload,
    LoginResponseData& outData);

bool parseWorldAuthRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId);

bool parseWorldAuthResponsePayload(
    const std::string& payload,
    WorldAuthResponseData& outData);

bool parseZoneAuthRequestPayload(
    const std::string& payload,
    HandoffToken& outHandoffToken,
    PlayerId& outCharacterId);

bool parseZoneAuthResponsePayload(
    const std::string& payload,
    ZoneAuthResponseData& outData);

// Character Management
bool parseCharacterListRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId);

bool parseCharacterListResponsePayload(
    const std::string& payload,
    CharacterListResponseData& outData);

bool parseCharacterCreateRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId,
    std::string& outName,
    std::string& outRace,
    std::string& outClass);

bool parseCharacterCreateResponsePayload(
    const std::string& payload,
    CharacterCreateResponseData& outData);

bool parseEnterWorldRequestPayload(
    const std::string& payload,
    SessionToken& outSessionToken,
    WorldId& outWorldId,
    std::uint64_t& outCharacterId);

bool parseEnterWorldResponsePayload(
    const std::string& payload,
    EnterWorldResponseData& outData);

// Movement
bool parseMovementIntentPayload(
    const std::string& payload,
    MovementIntentData& outData);

bool parsePlayerStateSnapshotPayload(
    const std::string& payload,
    PlayerStateSnapshotData& outData);
```

---

## Message Header

All messages are prefixed with a binary header:

```cpp
struct MessageHeader {
    std::uint16_t protocolVersion;  // Currently 1
    MessageType type;               // Message type enum (uint16_t)
    std::uint32_t payloadSize;      // Size of payload in bytes
    std::uint64_t reserved;         // Reserved for future use (set to 0)
};
```

**Total header size:** 16 bytes

---

## Complete Client Flow Example

### 1. Connect to LoginServer (127.0.0.1:7777)

```
Client ? LoginServer: LoginRequest
  Payload: "player1|mypassword|1.0.0|login"

LoginServer ? Client: LoginResponse
  Payload: "OK|123456789|1|1,MainWorld,127.0.0.1,7778,standard"
  
Client stores: sessionToken=123456789, worldId=1
```

### 2. Connect to WorldServer (127.0.0.1:7778)

```
Client ? WorldServer: CharacterListRequest
  Payload: "123456789|1"

WorldServer ? Client: CharacterListResponse
  Payload: "OK|1|42,TestWarrior,Human,Warrior,5"
  
Client displays character list, user selects character 42
```

### 3. Enter World

```
Client ? WorldServer: EnterWorldRequest
  Payload: "123456789|1|42"

WorldServer ? Client: EnterWorldResponse
  Payload: "OK|987654321|10|127.0.0.1|7780"
  
Client stores: handoffToken=987654321, zoneId=10
Client disconnects from WorldServer
```

### 4. Connect to ZoneServer (127.0.0.1:7780)

```
Client ? ZoneServer: ZoneAuthRequest
  Payload: "987654321|42"

ZoneServer ? Client: ZoneAuthResponse
  Payload: "OK|Welcome to East Freeport"
  
Client is now in zone, ready for gameplay
```

### 5. Movement Loop

```
Every frame (60 Hz):
  Client ? ZoneServer: MovementIntent
    Payload: "42|[seq]|[inputX]|[inputY]|[yaw]|[jump]|[time]"

Every 50ms (20 Hz):
  ZoneServer ? Client: PlayerStateSnapshot
    Payload: "[snapshotId]|[count]|[player1]|[player2]|..."
    
  Client applies server corrections to position
```

---

## Protocol Characteristics

### Wire Format
- **Encoding:** UTF-8 text strings
- **Delimiter:** Pipe character `|` for fields
- **Nested Delimiter:** Comma `,` for sub-fields (e.g., character data, player state)
- **Numbers:** Decimal string representation
- **Booleans:** `0` or `1`

### Performance
- **Text Protocol:** Easy to debug, human-readable
- **Overhead:** ~50-100 bytes per message (depending on content)
- **Future:** Binary protocol planned for production (10x reduction)

### Error Handling
- Parse functions return `bool` (true = success, false = failure)
- Response messages have `success` field
- Error responses include `errorCode` and `errorMessage`

### Server-Authoritative Model
- **Movement:** Client sends input only, server computes position
- **Validation:** Server validates all requests
- **Anti-Cheat:** Server rejects suspicious movement
- **State:** Server broadcasts authoritative state at 20 Hz

---

## Network Architecture

```
Client
  ? TCP
LoginServer (port 7777)
  ? SessionToken
  ? TCP
WorldServer (port 7778)
  ? HandoffToken
  ? TCP
ZoneServer (port 7780+)
  ? MovementIntent / PlayerStateSnapshot
```

---

## Common Error Codes

### Login Errors
- `AUTH_FAILED` - Invalid username or password
- `SERVER_FULL` - Server at capacity
- `BANNED` - Account banned

### World Errors
- `INVALID_SESSION` - Session token not recognized
- `WRONG_WORLD` - World ID mismatch

### Character Errors
- `CHARACTER_NOT_FOUND` - Character doesn't exist
- `NAME_TAKEN` - Character name already in use
- `INVALID_RACE` - Invalid race selection
- `INVALID_CLASS` - Invalid class selection
- `ACCESS_DENIED` - Character doesn't belong to this account

### Zone Errors
- `INVALID_HANDOFF` - Handoff token not recognized
- `ZONE_FULL` - Zone at capacity

---

## Implementation Notes

### Client-Side Prediction
For smooth movement, clients should:
1. Send MovementIntent to server
2. Immediately apply same input to local physics (prediction)
3. Render at predicted position
4. When PlayerStateSnapshot arrives, compare with prediction
5. If error > threshold, smoothly interpolate to server position

### Sequence Numbers
- Start at 0, increment with each MovementIntent
- Server can detect out-of-order or duplicate packets
- Client should ignore snapshots with lower snapshotId than last received

### Connection Management
- Use TCP for all connections
- Maintain separate connections to LoginServer, WorldServer, ZoneServer
- Disconnect from previous server when moving to next (except zone connection)

### Message Ordering
All messages on a single TCP connection are guaranteed to arrive in order.

---

## Protocol Version

**Current Version:** 1

All messages include protocol version in the MessageHeader. Future versions may:
- Add new message types
- Extend existing payloads with optional fields
- Introduce binary protocol option

Clients should check `protocolVersion` in received messages and handle mismatches gracefully.

---

## Contact & Support

This protocol is part of the REQ Backend project.

**Repository:** https://github.com/DrFishyStudios/REQ_Backend

---

*End of Protocol Reference*
