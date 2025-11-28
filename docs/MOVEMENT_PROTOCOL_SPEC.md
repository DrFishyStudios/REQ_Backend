# Movement Protocol Specification

## Overview

This document describes the movement message protocol for the REQ Backend zone-level simulation system. The protocol implements a **server-authoritative movement model** as described in GDD Section 14.3.

---

## Key Principles

### Server-Authoritative Model

**Client Responsibilities:**
- Send input vectors and button states
- Perform client-side prediction for smooth rendering
- Accept server corrections without question

**Server Responsibilities:**
- Receive client input (MovementIntent messages)
- Compute authoritative position and velocity
- Broadcast authoritative state (PlayerStateSnapshot messages)
- Handle physics simulation and collision detection

**Important:** Client position is **NEVER** trusted. Only input is sent from client to server.

---

## Message Types

### MovementIntent (Type 40)
**Direction:** Client ? ZoneServer  
**Frequency:** 20-60 Hz (configurable, typically matches client framerate)  
**Purpose:** Send player input to server for physics simulation

### PlayerStateSnapshot (Type 41)
**Direction:** ZoneServer ? Client  
**Frequency:** 20 Hz (configurable tick rate)  
**Purpose:** Broadcast authoritative player states to all clients in zone

---

## MovementIntent Message

### Payload Format
```
characterId|sequenceNumber|inputX|inputY|facingYawDegrees|isJumpPressed|clientTimeMs
```

### Field Definitions

| Field | Type | Range | Description |
|---|---|---|---|
| `characterId` | uint64 | >0 | Character ID sending the input |
| `sequenceNumber` | uint32 | 0..2³² | Increments per intent (for packet ordering) |
| `inputX` | float | -1.0..1.0 | Movement input on X axis (left/right) |
| `inputY` | float | -1.0..1.0 | Movement input on Y axis (forward/back) |
| `facingYawDegrees` | float | 0..360 | Facing direction in degrees (server normalizes) |
| `isJumpPressed` | int | 0 or 1 | Jump button state (0=released, 1=pressed) |
| `clientTimeMs` | uint32 | 0..2³² | Client timestamp in milliseconds (for telemetry) |

### Data Structure
```cpp
struct MovementIntentData {
    std::uint64_t characterId{ 0 };
    std::uint32_t sequenceNumber{ 0 };
    float inputX{ 0.0f };
    float inputY{ 0.0f };
    float facingYawDegrees{ 0.0f };
    bool isJumpPressed{ false };
    std::uint32_t clientTimeMs{ 0 };
};
```

### Example Payloads

**Standing Still:**
```
42|1|0.0|0.0|90.0|0|1234567890
```
- Character 42, sequence 1
- No input (0, 0)
- Facing 90 degrees (East)
- Not jumping
- Client time: 1234567890 ms

**Moving Forward-Right with Jump:**
```
42|2|0.707|0.707|45.0|1|1234567900
```
- Character 42, sequence 2
- Moving diagonal (normalized to 0.707, 0.707)
- Facing 45 degrees (Northeast)
- Jumping
- Client time: 1234567900 ms

**Turning in Place:**
```
42|3|0.0|0.0|180.0|0|1234567910
```
- Character 42, sequence 3
- No movement input
- Facing 180 degrees (South)
- Not jumping
- Client time: 1234567910 ms

### Usage Pattern

**Client sends MovementIntent:**
```cpp
using namespace req::shared::protocol;

MovementIntentData intent;
intent.characterId = myCharacterId;
intent.sequenceNumber = currentSequence++;
intent.inputX = getInputX();          // From keyboard/gamepad
intent.inputY = getInputY();          // From keyboard/gamepad
intent.facingYawDegrees = getCameraYaw();
intent.isJumpPressed = isJumpKeyPressed();
intent.clientTimeMs = getCurrentTimeMs();

std::string payload = buildMovementIntentPayload(intent);
connection->send(MessageType::MovementIntent, payload);
```

**Server receives and processes:**
```cpp
MovementIntentData intent;
if (parseMovementIntentPayload(payload, intent)) {
    // Apply input to character physics simulation
    applyMovementInput(intent.characterId, intent.inputX, intent.inputY);
    applyFacing(intent.characterId, intent.facingYawDegrees);
    if (intent.isJumpPressed) {
        tryJump(intent.characterId);
    }
}
```

---

## PlayerStateSnapshot Message

### Payload Format
```
snapshotId|playerCount|player1Data|player2Data|...
```

**Player Data Format (comma-separated):**
```
characterId,posX,posY,posZ,velX,velY,velZ,yawDegrees
```

### Field Definitions

| Field | Type | Description |
|---|---|---|
| `snapshotId` | uint64 | Incrementing snapshot identifier |
| `playerCount` | uint64 | Number of players in this snapshot |
| **Per Player:** | | |
| `characterId` | uint64 | Character ID |
| `posX` | float | World position X coordinate |
| `posY` | float | World position Y coordinate |
| `posZ` | float | World position Z coordinate |
| `velX` | float | Velocity X component (units/sec) |
| `velY` | float | Velocity Y component (units/sec) |
| `velZ` | float | Velocity Z component (units/sec) |
| `yawDegrees` | float | Facing direction in degrees (0-360) |

### Data Structures
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
    std::uint64_t snapshotId{ 0 };
    std::vector<PlayerStateEntry> players;
};
```

### Example Payloads

**Single Player Standing:**
```
5|1|42,100.5,200.0,10.0,0.0,0.0,0.0,90.0
```
- Snapshot ID: 5
- 1 player
- Character 42 at (100.5, 200.0, 10.0)
- Velocity (0, 0, 0) - standing still
- Facing 90 degrees

**Two Players Moving:**
```
6|2|42,100.5,200.0,10.0,5.0,0.0,0.0,90.0|43,150.0,200.0,10.0,0.0,3.0,0.0,180.0
```
- Snapshot ID: 6
- 2 players
- Character 42 at (100.5, 200.0, 10.0), moving +5.0 X velocity, facing 90°
- Character 43 at (150.0, 200.0, 10.0), moving +3.0 Y velocity, facing 180°

**Empty Zone:**
```
7|0
```
- Snapshot ID: 7
- 0 players (empty zone or all players out of range)

### Usage Pattern

**Server builds and broadcasts snapshot:**
```cpp
using namespace req::shared::protocol;

PlayerStateSnapshotData snapshot;
snapshot.snapshotId = currentSnapshotId++;

// Gather all player states
for (auto& character : activeCharacters) {
    PlayerStateEntry entry;
    entry.characterId = character.id;
    entry.posX = character.position.x;
    entry.posY = character.position.y;
    entry.posZ = character.position.z;
    entry.velX = character.velocity.x;
    entry.velY = character.velocity.y;
    entry.velZ = character.velocity.z;
    entry.yawDegrees = character.yaw;
    snapshot.players.push_back(entry);
}

std::string payload = buildPlayerStateSnapshotPayload(snapshot);
broadcastToAllClients(MessageType::PlayerStateSnapshot, payload);
```

**Client receives and applies:**
```cpp
PlayerStateSnapshotData snapshot;
if (parsePlayerStateSnapshotPayload(payload, snapshot)) {
    for (const auto& player : snapshot.players) {
        // Update rendering for this player
        if (player.characterId == myCharacterId) {
            // Apply server correction to my character
            correctPosition(player.posX, player.posY, player.posZ);
            correctVelocity(player.velX, player.velY, player.velZ);
        } else {
            // Update other players' positions
            updateOtherPlayer(player);
        }
    }
}
```

---

## Protocol Flow

### Typical Gameplay Loop

**Client Side (60 FPS):**
```
Frame 1:
  - Read input (WASD, mouse)
  - Send MovementIntent ? Server
  - Predict new position locally
  - Render at predicted position

Frame 2-3:
  - Continue sending MovementIntent
  - Continue prediction

Frame 4:
  - Receive PlayerStateSnapshot ? Server
  - Compare prediction with authoritative state
  - Apply correction if needed
  - Render at corrected position
```

**Server Side (20 TPS):**
```
Tick 1:
  - Receive MovementIntent from all clients
  - Update physics simulation
  - Compute new positions/velocities
  - Build PlayerStateSnapshot
  - Broadcast snapshot to all clients

Tick 2-N:
  - Repeat every 50ms (20 Hz)
```

### Sequence Diagram

```
Client                          ZoneServer
  |                                  |
  |  MovementIntent (seq=1)          |
  |--------------------------------->|
  |                                  |  Apply input
  |                                  |  Simulate physics
  |                                  |
  |  MovementIntent (seq=2)          |
  |--------------------------------->|
  |                                  |
  |         PlayerStateSnapshot      |  Tick boundary
  |<---------------------------------|
  |  Apply correction                |
  |                                  |
  |  MovementIntent (seq=3)          |
  |--------------------------------->|
  |                                  |
```

---

## Implementation Notes

### Input Normalization

**Client Responsibility:**
- Normalize diagonal movement: `sqrt(inputX² + inputY²) ? 1.0`
- Clamp individual axes to [-1.0, 1.0]

**Example:**
```cpp
// Raw input
float rawX = keyboard.isKeyDown('D') ? 1.0f : (keyboard.isKeyDown('A') ? -1.0f : 0.0f);
float rawY = keyboard.isKeyDown('W') ? 1.0f : (keyboard.isKeyDown('S') ? -1.0f : 0.0f);

// Normalize diagonal movement
float magnitude = sqrt(rawX * rawX + rawY * rawY);
if (magnitude > 1.0f) {
    inputX = rawX / magnitude;
    inputY = rawY / magnitude;
} else {
    inputX = rawX;
    inputY = rawY;
}
```

### Server Validation

**Server Should:**
- Clamp input values to valid ranges
- Normalize yaw to 0-360 range
- Validate sequence numbers (detect/handle packet loss)
- Rate-limit MovementIntent messages (prevent spam)

**Example:**
```cpp
// Normalize yaw
while (intent.facingYawDegrees < 0.0f) intent.facingYawDegrees += 360.0f;
while (intent.facingYawDegrees >= 360.0f) intent.facingYawDegrees -= 360.0f;

// Clamp input
intent.inputX = std::clamp(intent.inputX, -1.0f, 1.0f);
intent.inputY = std::clamp(intent.inputY, -1.0f, 1.0f);
```

### Client-Side Prediction

**Why Predict?**
- Reduces perceived latency
- Keeps movement smooth despite network delays

**How to Predict:**
1. Client sends MovementIntent to server
2. Client immediately applies same input to local physics
3. Client renders at predicted position
4. When PlayerStateSnapshot arrives, compare:
   - If close: Keep prediction
   - If far: Smoothly interpolate to server position

**Example Correction:**
```cpp
Vector3 serverPos = snapshot.getPlayerPosition(myCharacterId);
Vector3 predictedPos = myLocalPosition;
Vector3 error = serverPos - predictedPos;

if (error.length() > CORRECTION_THRESHOLD) {
    // Snap to server position (large error)
    myLocalPosition = serverPos;
} else {
    // Smooth interpolation (small error)
    myLocalPosition = lerp(predictedPos, serverPos, CORRECTION_RATE);
}
```

### Snapshot Frequency

**Recommended Rates:**
- **High fidelity:** 30-60 Hz (competitive shooters)
- **Standard:** 20 Hz (MMORPGs, most games)
- **Low bandwidth:** 10 Hz (mobile, slow connections)

**Trade-offs:**
- Higher rate = smoother movement, more bandwidth
- Lower rate = choppier movement, less bandwidth

**Current Implementation:** 20 Hz (configurable)

---

## Error Handling

### Parse Errors

**MovementIntent Parse Failures:**
```
[ERROR] [Protocol] MovementIntent: expected 7 fields, got 5
[ERROR] [Protocol] MovementIntent: failed to parse inputX
```

**PlayerStateSnapshot Parse Failures:**
```
[ERROR] [Protocol] PlayerStateSnapshot: expected at least 2 fields, got 1
[WARNING] [Protocol] PlayerStateSnapshot: playerCount mismatch - expected 3, got 2 entries
[ERROR] [Protocol] PlayerStateSnapshot: player entry 0 malformed (expected 8 fields, got 6)
```

### Tolerance Policy

**Strict Validation:**
- Field count mismatches ? Parse failure
- Invalid numeric conversions ? Parse failure
- Missing required fields ? Parse failure

**Tolerant Behavior:**
- PlayerCount mismatch ? Log warning, continue with actual count
- Extra fields ? Ignored
- Yaw out of range ? Server normalizes to 0-360

---

## Testing

### Unit Tests

**MovementIntent:**
```cpp
// Test basic parsing
MovementIntentData intent;
std::string payload = "42|1|0.5|-0.3|90.0|1|1234567890";
assert(parseMovementIntentPayload(payload, intent));
assert(intent.characterId == 42);
assert(intent.inputX == 0.5f);
assert(intent.isJumpPressed == true);

// Test building
MovementIntentData data{42, 1, 0.5f, -0.3f, 90.0f, true, 1234567890};
std::string built = buildMovementIntentPayload(data);
assert(built == payload);
```

**PlayerStateSnapshot:**
```cpp
// Test parsing with multiple players
std::string payload = "5|2|42,100.0,200.0,10.0,0.0,0.0,0.0,90.0|43,150.0,200.0,10.0,1.5,0.0,0.0,180.0";
PlayerStateSnapshotData snapshot;
assert(parsePlayerStateSnapshotPayload(payload, snapshot));
assert(snapshot.snapshotId == 5);
assert(snapshot.players.size() == 2);
assert(snapshot.players[0].characterId == 42);
assert(snapshot.players[1].velX == 1.5f);
```

### Integration Tests

**Send and Receive:**
1. Client sends MovementIntent
2. Server receives and parses
3. Server simulates physics
4. Server builds PlayerStateSnapshot
5. Client receives and applies

---

## Performance Considerations

### Bandwidth

**MovementIntent (per message):**
- Typical payload: ~50 bytes
- At 60 Hz: 3 KB/s per client
- 100 clients: 300 KB/s upstream to server

**PlayerStateSnapshot (per message):**
- Header: ~10 bytes
- Per player: ~60 bytes
- 10 players: ~610 bytes
- At 20 Hz: 12.2 KB/s per client
- 100 clients: 1.22 MB/s downstream from server

**Optimization Strategies:**
- Delta compression (only send changes)
- Interest management (only send nearby players)
- Binary protocol (replace text with binary)
- Prediction (reduce snapshot frequency)

### CPU

**Parsing Cost:**
- Text parsing: ~1-2 ?s per message (modern CPU)
- Acceptable for 1000s of messages per second

**Future Optimization:**
- Binary protocol: 10x faster parsing
- SIMD for batch processing
- Zero-copy buffers

---

## Future Enhancements

### Planned Features
- [ ] Delta compression for snapshots
- [ ] Interest management (area-of-interest)
- [ ] Lag compensation
- [ ] Jitter buffer
- [ ] Packet loss recovery
- [ ] Binary protocol option
- [ ] Snapshot interpolation hints

### GDD Alignment
See GDD Section 14.3 for detailed movement system design.

---

## Quick Reference

### Message Type IDs
```cpp
MovementIntent      = 40  // Client ? ZoneServer
PlayerStateSnapshot = 41  // ZoneServer ? Client
```

### Payload Formats
```
MovementIntent:
  characterId|sequenceNumber|inputX|inputY|facingYawDegrees|isJumpPressed|clientTimeMs

PlayerStateSnapshot:
  snapshotId|playerCount|char1,posX,posY,posZ,velX,velY,velZ,yaw|char2,...
```

### Helper Functions
```cpp
// Build
std::string buildMovementIntentPayload(const MovementIntentData& data);
std::string buildPlayerStateSnapshotPayload(const PlayerStateSnapshotData& data);

// Parse
bool parseMovementIntentPayload(const std::string& payload, MovementIntentData& outData);
bool parsePlayerStateSnapshotPayload(const std::string& payload, PlayerStateSnapshotData& outData);
```
