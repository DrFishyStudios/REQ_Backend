# ZoneServer Multi-Player Architecture

## Overview
This document describes the ZoneServer's multi-player handling architecture, ensuring robust player lifecycle management and correct snapshot broadcasting.

---

## A) ZonePlayer Representation

### Data Structure
```cpp
struct ZonePlayer {
    std::uint64_t accountId;
    std::uint64_t characterId;  // PRIMARY KEY
    
    // Connection for sending messages
    std::shared_ptr<req::shared::net::Connection> connection;
    
    // Position and movement state
    float posX, posY, posZ;
    float velX, velY, velZ;
    float yawDegrees;
    
    // Anti-cheat validation
    float lastValidPosX, lastValidPosY, lastValidPosZ;
    
    // Client input
    float inputX, inputY;
    bool isJumpPressed;
    std::uint32_t lastSequenceNumber;
    
    // Lifecycle flags
    bool isInitialized;
    bool isDirty;  // Position changed since last save
};
```

### Container Design
**Primary Map:** `std::unordered_map<uint64_t, ZonePlayer> players_;`
- **Key:** `characterId` (stable, unique identifier)
- **Value:** `ZonePlayer` struct with all state

**Reverse Lookup:** `std::unordered_map<ConnectionPtr, uint64_t> connectionToCharacterId_;`
- **Purpose:** Find `characterId` when connection closes
- **Key:** `shared_ptr<Connection>`
- **Value:** `characterId`

**Why characterId as Primary Key?**
- ? Stable across reconnections
- ? Persists after character creation
- ? Matches database/file storage keys
- ? Used in protocol messages (MovementIntent, snapshots)
- ? Prevents duplicate logins for same character

---

## B) ZonePlayer Lifecycle

### 1. Creation (ZoneAuth Success)

**Trigger:** `ZoneAuthRequest` message received and validated

**Process:**
1. Parse `handoffToken` and `characterId` from request
2. Validate handoff token (stub validation for now)
3. Load character data from `CharacterStore`
4. Check for duplicate login (remove old entry if exists)
5. Create `ZonePlayer` instance:
   - Set `characterId` and `accountId`
   - Store `connection` pointer
   - Initialize position from character data (or safe spawn)
   - Set `isInitialized = true`, `isDirty = false`
6. Insert into `players_[characterId]`
7. Insert into `connectionToCharacterId_[connection]`
8. Send `ZoneAuthResponse` success

**Logging:**
```
[INFO] [zone] [ZonePlayer created] characterId=1, accountId=1, zoneId=10, 
       pos=(100.5,200.0,10.0), yaw=90.0, active_players=2
```

**Code:**
```cpp
// In handleMessage() for ZoneAuthRequest
ZonePlayer player;
player.characterId = characterId;
player.accountId = character->accountId;
player.connection = connection;
spawnPlayer(*character, player);
// ... initialize other fields ...
players_[characterId] = player;
connectionToCharacterId_[connection] = characterId;
```

---

### 2. Update (During Simulation)

**Trigger:** 
- `MovementIntent` messages from client
- Simulation tick (20Hz)

**Process:**
1. **MovementIntent Handler:**
   - Find `ZonePlayer` by `characterId` from message
   - Validate sequence number (reject old/duplicate)
   - Update input state (`inputX`, `inputY`, `isJumpPressed`, `yawDegrees`)

2. **Simulation Tick (`updateSimulation()`):**
   - Iterate all `players_`
   - For each initialized player:
     - Apply physics (movement, gravity, jumping)
     - Validate movement (anti-cheat checks)
     - Update position (`posX`, `posY`, `posZ`)
     - Set `isDirty = true` if moved

**Code:**
```cpp
void ZoneServer::updateSimulation(float dt) {
    for (auto& [characterId, player] : players_) {
        if (!player.isInitialized) continue;
        
        // Apply input to velocity
        player.velX = player.inputX * MAX_SPEED;
        player.velY = player.inputY * MAX_SPEED;
        
        // Apply gravity/jumping
        // ... physics code ...
        
        // Integrate position
        player.posX += player.velX * dt;
        player.posY += player.velY * dt;
        player.posZ += player.velZ * dt;
        
        player.isDirty = true;
    }
}
```

---

### 3. Removal (Disconnect or Timeout)

**Trigger:** 
- Connection closed (network error, client quit)
- Explicit disconnect message (future)
- Server shutdown

**Process:**
1. Connection error detected ? `Connection::close()` called
2. `Connection::close()` calls `onDisconnect_` handler
3. `ZoneServer::onConnectionClosed()` called:
   - Look up `characterId` via `connectionToCharacterId_`
   - Call `removePlayer(characterId)`
4. `removePlayer()`:
   - Find `ZonePlayer` in `players_`
   - Call `savePlayerPosition()` (final save)
   - Remove from `connectionToCharacterId_`
   - Remove from `players_`
5. Remove connection from `connections_` vector

**Logging:**
```
[INFO] [zone] [Connection] Connection closed, checking for associated ZonePlayer
[INFO] [zone] [Connection] Found characterId=1 for closed connection
[INFO] [zone] [DISCONNECT] Saving final position for characterId=1
[INFO] [zone] [SAVE] Position saved: characterId=1, zoneId=10, pos=(150.0,250.0,15.0), yaw=180.0
[INFO] [zone] [ZonePlayer removed] characterId=1, reason=disconnect, remaining_players=1
[INFO] [zone] [Connection] Connection cleanup complete, total connections=1
```

**Code:**
```cpp
void ZoneServer::onConnectionClosed(ConnectionPtr connection) {
    auto it = connectionToCharacterId_.find(connection);
    if (it != connectionToCharacterId_.end()) {
        std::uint64_t characterId = it->second;
        removePlayer(characterId);
        connectionToCharacterId_.erase(it);
    }
    connections_.erase(std::remove(connections_.begin(), connections_.end(), connection),
                      connections_.end());
}

void ZoneServer::removePlayer(std::uint64_t characterId) {
    auto it = players_.find(characterId);
    if (it == players_.end()) return;
    
    savePlayerPosition(characterId);
    
    const ZonePlayer& player = it->second;
    if (player.connection) {
        connectionToCharacterId_.erase(player.connection);
    }
    
    players_.erase(it);
}
```

---

## C) PlayerStateSnapshot Broadcasting

### Purpose
- **Server-authoritative:** Server owns the truth about all player positions
- **Broadcast to all:** Each client receives state of ALL players in zone
- **No client prediction yet:** Clients render exactly what server says

### Snapshot Building (20Hz)

**Trigger:** `onTick()` ? `broadcastSnapshots()`

**Process:**
1. Check if `players_` is empty ? skip if no players
2. Increment `snapshotCounter_`
3. Iterate all `players_`:
   - Skip if `!isInitialized`
   - Create `PlayerStateEntry` with:
     - `characterId`
     - `posX, posY, posZ`
     - `velX, velY, velZ`
     - `yawDegrees`
   - Add to `snapshot.players` vector
4. Build payload string via `buildPlayerStateSnapshotPayload()`
5. Broadcast to ALL connections in `connections_` vector

**Key Point:** Snapshot includes ALL active players, not just local player

**Code:**
```cpp
void ZoneServer::broadcastSnapshots() {
    if (players_.empty()) return;
    
    PlayerStateSnapshotData snapshot;
    snapshot.snapshotId = ++snapshotCounter_;
    
    // Include ALL players
    for (const auto& [characterId, player] : players_) {
        if (!player.isInitialized) continue;
        
        PlayerStateEntry entry;
        entry.characterId = player.characterId;
        entry.posX = player.posX;
        entry.posY = player.posY;
        entry.posZ = player.posZ;
        entry.velX = player.velX;
        entry.velY = player.velY;
        entry.velZ = player.velZ;
        entry.yawDegrees = player.yawDegrees;
        
        snapshot.players.push_back(entry);
    }
    
    // Broadcast to ALL connections
    std::string payloadStr = buildPlayerStateSnapshotPayload(snapshot);
    ByteArray payloadBytes(payloadStr.begin(), payloadStr.end());
    
    for (auto& connection : connections_) {
        if (connection) {
            connection->send(MessageType::PlayerStateSnapshot, payloadBytes);
        }
    }
}
```

### Logging

**Periodic Summary (Every 100 Snapshots = ~5 seconds):**
```
[INFO] [zone] [Snapshot] Building snapshot 100 for 3 active player(s)
[INFO] [zone] [Snapshot] Broadcast snapshot 100 with 3 player(s) to 3 connection(s)
```

**Detailed Debug (Optional, Commented Out):**
```cpp
// Uncomment in broadcastSnapshots() for per-player details
[INFO] [zone] [Snapshot] Snapshot 100 contents:
[INFO] [zone]   characterId=1, pos=(100.5,200.0,10.0), vel=(7.0,0.0,0.0), yaw=90.0
[INFO] [zone]   characterId=2, pos=(50.0,150.0,0.0), vel=(0.0,7.0,0.0), yaw=0.0
[INFO] [zone]   characterId=3, pos=(200.0,100.0,5.0), vel=(0.0,0.0,0.0), yaw=180.0
```

---

## Multi-Player Scenarios

### Scenario 1: Two Players in Same Zone

**Setup:**
1. Player A (characterId=1) enters zone
2. Player B (characterId=2) enters zone

**Expected Behavior:**
- `players_` map has 2 entries: `{1 ? ZonePlayerA, 2 ? ZonePlayerB}`
- Each snapshot includes BOTH players
- Player A's client receives snapshots with entries for A and B
- Player B's client receives snapshots with entries for A and B

**Verification:**
```
[INFO] [zone] [ZonePlayer created] characterId=1, ..., active_players=1
[INFO] [zone] [ZonePlayer created] characterId=2, ..., active_players=2
[INFO] [zone] [Snapshot] Broadcast snapshot 50 with 2 player(s) to 2 connection(s)
```

---

### Scenario 2: Player Disconnect with Others Still Active

**Setup:**
1. Player A, B, C in zone (3 players)
2. Player B disconnects

**Expected Behavior:**
1. Connection close detected for Player B
2. `removePlayer(2)` called
3. `players_` now has 2 entries: `{1, 3}`
4. Snapshots now include only Player A and C
5. Player A and C receive snapshots with 2 entries

**Verification:**
```
[INFO] [zone] [Connection] Found characterId=2 for closed connection
[INFO] [zone] [ZonePlayer removed] characterId=2, reason=disconnect, remaining_players=2
[INFO] [zone] [Snapshot] Broadcast snapshot 150 with 2 player(s) to 2 connection(s)
```

---

### Scenario 3: Duplicate Login Prevention

**Setup:**
1. Player A (characterId=1) connected and in zone
2. Player A tries to login again (same characterId)

**Expected Behavior:**
1. Second ZoneAuthRequest received with characterId=1
2. Check finds existing entry in `players_[1]`
3. Call `removePlayer(1)` to clean up old entry
4. Create new `ZonePlayer` for characterId=1
5. Only one entry remains in `players_`

**Verification:**
```
[INFO] [zone] [ZONEAUTH] Character already in zone: characterId=1, removing old entry
[INFO] [zone] [ZonePlayer removed] characterId=1, reason=disconnect, remaining_players=0
[INFO] [zone] [ZonePlayer created] characterId=1, ..., active_players=1
```

---

## Testing with Multiple Clients

### Manual Test Procedure

**Step 1: Start Servers**
```bash
cd x64/Debug
.\REQ_LoginServer.exe
.\REQ_WorldServer.exe
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="Test Zone"
```

**Step 2: Connect Two TestClients**

Terminal 1:
```bash
.\REQ_TestClient.exe
# Login as testuser, enter zone, move with: w, a, s, d
```

Terminal 2:
```bash
.\REQ_TestClient.exe
# Login as player1 (or create new account), enter zone, move
```

**Step 3: Monitor ZoneServer Logs**

Look for:
1. **Player Creation:**
   ```
   [ZonePlayer created] characterId=1, ..., active_players=1
   [ZonePlayer created] characterId=2, ..., active_players=2
   ```

2. **Snapshot Broadcasting:**
   ```
   [Snapshot] Broadcast snapshot 100 with 2 player(s) to 2 connection(s)
   ```

3. **Movement Updates:**
   Each client should see BOTH players moving in snapshots

**Step 4: Test Disconnect**

In Terminal 1, quit TestClient (type `q`)

Check ZoneServer logs:
```
[Connection] Found characterId=1 for closed connection
[ZonePlayer removed] characterId=1, reason=disconnect, remaining_players=1
```

In Terminal 2, TestClient should continue receiving snapshots with only 1 player

---

## Common Issues and Solutions

### Issue: Snapshots Only Include Local Player
**Symptom:** Client only sees themselves, not other players

**Cause:** Filtering logic in `broadcastSnapshots()` or client-side parsing

**Fix:** Verify `broadcastSnapshots()` iterates ALL `players_`, not just current connection

### Issue: Stale Players After Disconnect
**Symptom:** Disconnected player still appears in snapshots

**Cause:** `onConnectionClosed()` not called or `removePlayer()` not removing entry

**Fix:** 
- Check `Connection::close()` calls `onDisconnect_` handler
- Verify disconnect handler is set in `handleNewConnection()`
- Add logging to track disconnect flow

### Issue: Duplicate characterId in players_
**Symptom:** Two entries with same characterId

**Cause:** Missing duplicate check in ZoneAuth handler

**Fix:** Add duplicate check before creating ZonePlayer:
```cpp
if (players_.find(characterId) != players_.end()) {
    removePlayer(characterId);  // Clean up old entry
}
```

---

## Future Enhancements

### Planned Features
- [ ] **Interest Management:** Only send snapshots for nearby players
- [ ] **Snapshot Delta Compression:** Only send changed fields
- [ ] **Client Prediction:** Allow local movement prediction with server correction
- [ ] **Player Visibility Filtering:** Hide stealthed/invisible players
- [ ] **Zone Capacity Limits:** Reject new players when zone full

### Planned Optimizations
- [ ] **Batch Snapshot Building:** Reuse single snapshot for all connections
- [ ] **Connection Pooling:** Avoid vector erase/remove overhead
- [ ] **Spatial Indexing:** Quadtree/grid for fast range queries

---

## Code References

### Key Files
- `REQ_ZoneServer/include/req/zone/ZoneServer.h` - ZonePlayer struct, containers
- `REQ_ZoneServer/src/ZoneServer.cpp` - Lifecycle, simulation, snapshot logic
- `REQ_Shared/include/req/shared/Connection.h` - Disconnect handler interface
- `REQ_Shared/src/Connection.cpp` - Disconnect handler implementation

### Key Methods
```cpp
// Lifecycle
void handleMessage(...)               // ZoneAuthRequest handler
void onConnectionClosed(...)          // Disconnect detection
void removePlayer(uint64_t)           // Cleanup on disconnect

// Simulation
void updateSimulation(float dt)       // Physics update for all players

// Snapshot Broadcasting
void broadcastSnapshots()             // Build and send to all clients
```

---

## Quick Verification Commands

### Check Active Players
```cpp
// In ZoneServer logs, look for:
[ZonePlayer created] characterId=X, ..., active_players=N
[ZonePlayer removed] characterId=X, ..., remaining_players=N
```

### Verify Multi-Player Snapshots
```cpp
// Every ~5 seconds (100 snapshots):
[Snapshot] Broadcast snapshot 100 with N player(s) to N connection(s)
```

### Inspect Snapshot Contents (Enable Debug Logging)
Uncomment debug block in `broadcastSnapshots()`:
```cpp
[Snapshot] Snapshot 100 contents:
  characterId=1, pos=(...), vel=(...), yaw=...
  characterId=2, pos=(...), vel=(...), yaw=...
```

---

## Summary

? **ZonePlayer Container:**
- Primary key: `characterId` (stable, unique)
- Stores connection pointer for message sending
- Supports efficient lookup and iteration

? **Lifecycle Management:**
- Creation: On ZoneAuth success with full logging
- Update: Via MovementIntent + simulation tick
- Removal: On disconnect with final save and cleanup

? **Snapshot Broadcasting:**
- Includes ALL active players (no filtering yet)
- Sent to ALL connections at 20Hz
- Consistent schema across server and client

? **Multi-Player Ready:**
- Multiple players can coexist in same zone
- Each client receives state of all players
- Proper cleanup prevents stale data

**Architecture is robust and ready for multi-player testing! ??**
