# Zone Entry and Position Persistence - Implementation Guide

## Overview

ZoneServer now implements intelligent character spawn logic with position persistence, providing an MMO-style experience where players spawn at their last known location or at a configurable safe point for first-time visits.

---

## Key Features

? **Smart Spawn Logic** - Restores last position or uses zone safe point  
? **Position Auto-Save** - Periodic saving every 30 seconds (configurable)  
? **Disconnect Persistence** - Saves position on client disconnect  
? **Character Data Integration** - Loads/saves full character JSON  
? **Backward Compatible** - Handles characters without saved positions  

---

## Architecture

### Data Flow

```
Character JSON (disk)
        ?
   ZoneServer CharacterStore
        ?
   Spawn Logic (spawnPlayer)
        ?
   ZonePlayer (in-memory)
        ?
   Position Updates (simulation tick)
        ?
   Auto-Save Timer / Disconnect
        ?
   Character JSON (disk)
```

### Components

**ZoneServer** - Main server managing all zone players  
**Character Store** - Loads/saves character JSON files  
**ZonePlayer** - In-memory state for active players  
**ZoneConfig** - Zone-specific safe spawn configuration  

---

## Spawn Logic Rules

### Rule 1: Restore Saved Position

**Conditions:**
- `character.lastZoneId` matches current `zoneId`
- Saved position is non-zero (at least one coordinate != 0.0f)

**Action:**
- Restore `posX`, `posY`, `posZ`, `heading` from character JSON
- Log: `[SPAWN] Restored position for characterId=X`

**Example:**
```
Character last in zone 10 at (100.5, 200.0, 10.0)
? Returns to zone 10
? Spawns at (100.5, 200.0, 10.0)
```

### Rule 2: Use Safe Spawn Point

**Conditions:**
- `lastZoneId` != current `zoneId` (first visit or zone change)
- OR saved position is all zeros (new character)

**Action:**
- Use zone's configured safe spawn point (`safeX`, `safeY`, `safeZ`, `safeYaw`)
- Update character's `lastWorldId`, `lastZoneId`, position to reflect new location
- Save character JSON with updated position
- Log: `[SPAWN] Using safe spawn point for characterId=X (first visit or zone mismatch)`

**Example:**
```
Character last in zone 20
? Enters zone 10 for first time
? Spawns at zone 10's safe point (0, 0, 0)
? Character JSON updated with lastZoneId=10
```

---

## Position Persistence

### Auto-Save (Periodic)

**Frequency:** Every 30 seconds (configurable via `ZoneConfig.autosaveIntervalSec`)

**Logic:**
1. Timer fires every 30 seconds
2. Iterate all active ZonePlayers
3. Skip players where `isDirty = false` (no movement since last save)
4. For each dirty player:
   - Load character JSON from disk
   - Update `lastWorldId`, `lastZoneId`, position fields
   - Save character JSON
   - Clear `isDirty` flag

**Logging:**
```
[AUTOSAVE] Saved positions for 3 player(s), skipped 1
```

**Benefits:**
- Prevents significant progress loss on server crash
- Reduces disk I/O by only saving dirty (moved) characters
- Configurable interval based on server needs

### Disconnect Save

**Trigger:** Client disconnects or connection lost

**Logic:**
1. Detect connection closure
2. Find characterId for connection
3. Save final position to disk
4. Remove player from active players map
5. Clean up connection mapping

**Logging:**
```
[DISCONNECT] Saving final position for characterId=42
[SAVE] Position saved: characterId=42, zoneId=10, pos=(150.3,220.1,5.0), yaw=90.0
[DISCONNECT] Player removed: characterId=42, remaining players=2
```

**Guarantees:**
- Position always saved before player removal
- No position data loss on clean disconnect

### Save Triggers Summary

| Trigger | Frequency | Dirty Check | Notes |
|---------|-----------|-------------|-------|
| Zone Entry (first visit) | Once | N/A | Updates lastZoneId and position |
| Auto-Save | Every 30s | Yes | Only saves if `isDirty` |
| Disconnect | Once | No | Always saves final position |
| Server Shutdown | Once | No | TODO: implement graceful shutdown |

---

## Zone Configuration

### ZoneConfig Structure

```cpp
struct ZoneConfig {
    uint32_t zoneId;
    string zoneName;
    
    // Safe spawn point (for first-time entry)
    float safeX;
    float safeY;
    float safeZ;
    float safeYaw;
    
    // Position auto-save interval (seconds)
    float autosaveIntervalSec;
};
```

### Default Values

```cpp
zoneConfig_.safeX = 0.0f;
zoneConfig_.safeY = 0.0f;
zoneConfig_.safeZ = 0.0f;
zoneConfig_.safeYaw = 0.0f;
zoneConfig_.autosaveIntervalSec = 30.0f;
```

### Configuration Methods

**Set via code:**
```cpp
ZoneConfig config;
config.safeX = 100.0f;
config.safeY = 200.0f;
config.safeZ = 10.0f;
config.safeYaw = 90.0f;
config.autosaveIntervalSec = 60.0f;

zoneServer.setZoneConfig(config);
```

**TODO: Load from zone config JSON:**
```json
{
  "zone_id": 10,
  "zone_name": "East Freeport",
  "safe_spawn": {
    "x": 100.0,
    "y": 200.0,
    "z": 10.0,
    "yaw": 90.0
  },
  "autosave_interval_sec": 30.0
}
```

---

## Character JSON Schema Updates

### New Fields Used

| Field | Type | Usage in Spawn Logic |
|-------|------|---------------------|
| `lastWorldId` | uint32 | Updated on zone entry |
| `lastZoneId` | uint32 | Used to determine if restoring position |
| `positionX` | float | Restored if lastZoneId matches |
| `positionY` | float | Restored if lastZoneId matches |
| `positionZ` | float | Restored if lastZoneId matches |
| `heading` | float | Restored if lastZoneId matches |

### Example Character JSON

**After Zone Entry:**
```json
{
  "character_id": 42,
  "name": "Arthas",
  "race": "Human",
  "class": "Paladin",
  "level": 5,
  
  "last_world_id": 1,
  "last_zone_id": 10,
  
  "position_x": 150.3,
  "position_y": 220.1,
  "position_z": 5.0,
  "heading": 90.0
}
```

**First-Time Entry (Before Save):**
```json
{
  "character_id": 42,
  "last_world_id": 0,
  "last_zone_id": 0,
  "position_x": 0.0,
  "position_y": 0.0,
  "position_z": 0.0,
  "heading": 0.0
}
```

**After Safe Spawn:**
```json
{
  "character_id": 42,
  "last_world_id": 1,
  "last_zone_id": 10,
  "position_x": 0.0,
  "position_y": 0.0,
  "position_z": 0.0,
  "heading": 0.0
}
```

---

## Logging Reference

### Spawn Logs

**Restored Position:**
```
[SPAWN] Restored position for characterId=42: pos=(150.3,220.1,5.0), yaw=90.0
```

**Safe Spawn:**
```
[SPAWN] Using safe spawn point for characterId=42 (first visit or zone mismatch): pos=(0.0,0.0,0.0), yaw=0.0
[SPAWN] Updated character lastZone/position: characterId=42, lastZoneId=10
```

### Save Logs

**Successful Save:**
```
[SAVE] Position saved: characterId=42, zoneId=10, pos=(150.3,220.1,5.0), yaw=90.0
```

**Save Failure:**
```
[SAVE] Failed to save character position: characterId=42
```

**Character Not Found:**
```
[SAVE] Cannot save position - character not found: characterId=42
```

### Auto-Save Logs

**Periodic Save:**
```
[AUTOSAVE] Saved positions for 3 player(s), skipped 1
```

**No Dirty Players:**
```
(No log - autosave is silent if no dirty players)
```

### Disconnect Logs

**Clean Disconnect:**
```
[DISCONNECT] Saving final position for characterId=42
[SAVE] Position saved: characterId=42, zoneId=10, pos=(150.3,220.1,5.0), yaw=90.0
[DISCONNECT] Player removed: characterId=42, remaining players=2
```

---

## Implementation Details

### ZonePlayer Dirty Flag

**Purpose:** Track if player has moved since last save

**Set Conditions:**
- Position changed by > 0.01 units (threshold to avoid tiny movement spam)

**Clear Conditions:**
- Successful position save (auto-save or disconnect)

**Usage:**
```cpp
// In updateSimulation():
if (dist > 0.01f) {
    player.isDirty = true;
}

// In savePlayerPosition():
if (characterStore_.saveCharacter(*character)) {
    playerIt->second.isDirty = false;
}
```

**Benefits:**
- Reduces unnecessary disk I/O
- Only saves characters that actually moved
- Prevents autosave spam for idle players

### Position Validation

**Zero-Position Check:**
```cpp
bool hasValidPosition = (character.positionX != 0.0f || 
                        character.positionY != 0.0f || 
                        character.positionZ != 0.0f);
```

**Why:** Distinguishes between:
- New character (all zeros)
- Character saved at actual (0, 0, 0) position

**Caveat:** If (0, 0, 0) is a valid position in your game, consider using a sentinel value like (-9999, -9999, -9999) or add a `hasValidPosition` flag to character JSON.

### Character Store Integration

**Load on Zone Entry:**
```cpp
auto character = characterStore_.loadById(characterId);
if (!character.has_value()) {
    // Send error response
    return;
}
```

**Save on Position Update:**
```cpp
character->lastWorldId = worldId_;
character->lastZoneId = zoneId_;
character->positionX = player.posX;
character->positionY = player.posY;
character->positionZ = player.posZ;
character->heading = player.yawDegrees;

if (characterStore_.saveCharacter(*character)) {
    // Success
}
```

**Thread Safety:** Currently single-threaded (one io_context). For multi-threaded servers, add locking around character store operations.

---

## Testing Scenarios

### Test 1: First-Time Zone Entry

**Setup:**
1. Create character via WorldServer
2. Character JSON has `lastZoneId = 0` or `lastZoneId = 10` (default starting zone)

**Steps:**
1. Enter zone 10
2. Check spawn position

**Expected:**
- Position = zone safe spawn point (0, 0, 0 by default)
- Character JSON updated with `lastZoneId = 10`

**Logs:**
```
[SPAWN] Using safe spawn point for characterId=42 (first visit or zone mismatch)
[SPAWN] Updated character lastZone/position: characterId=42, lastZoneId=10
```

### Test 2: Returning to Same Zone

**Setup:**
1. Character previously logged out at (100, 200, 10) in zone 10
2. Character JSON has `lastZoneId = 10`, `position_x = 100`, etc.

**Steps:**
1. Enter zone 10
2. Check spawn position

**Expected:**
- Position = (100, 200, 10)
- No character JSON update (already correct)

**Logs:**
```
[SPAWN] Restored position for characterId=42: pos=(100.0,200.0,10.0), yaw=0.0
```

### Test 3: Zone Change

**Setup:**
1. Character last in zone 20
2. Character JSON has `lastZoneId = 20`

**Steps:**
1. Enter zone 10 (different zone)
2. Check spawn position

**Expected:**
- Position = zone 10 safe spawn point
- Character JSON updated with `lastZoneId = 10`

**Logs:**
```
[SPAWN] Using safe spawn point for characterId=42 (first visit or zone mismatch)
[SPAWN] Updated character lastZone/position: characterId=42, lastZoneId=10
```

### Test 4: Movement and Auto-Save

**Setup:**
1. Character logged in at (0, 0, 0)
2. Auto-save interval = 30s

**Steps:**
1. Move character to (50, 50, 0)
2. Wait 30 seconds
3. Check character JSON

**Expected:**
- After 30s: Position saved to disk
- Character JSON reflects new position

**Logs:**
```
[AUTOSAVE] Saved positions for 1 player(s), skipped 0
[SAVE] Position saved: characterId=42, zoneId=10, pos=(50.0,50.0,0.0), yaw=0.0
```

### Test 5: Disconnect Save

**Setup:**
1. Character logged in and moved to (100, 100, 5)

**Steps:**
1. Disconnect client
2. Check character JSON

**Expected:**
- Position saved immediately on disconnect
- Character JSON reflects final position

**Logs:**
```
[DISCONNECT] Saving final position for characterId=42
[SAVE] Position saved: characterId=42, zoneId=10, pos=(100.0,100.0,5.0), yaw=90.0
[DISCONNECT] Player removed: characterId=42, remaining players=0
```

### Test 6: Multiple Players

**Setup:**
1. 3 players in zone
2. 2 moving, 1 idle

**Steps:**
1. Wait for auto-save
2. Check logs

**Expected:**
- Only 2 moving players saved
- 1 idle player skipped

**Logs:**
```
[AUTOSAVE] Saved positions for 2 player(s), skipped 1
```

---

## Performance Considerations

### Disk I/O

**Auto-Save Frequency:**
- Default: 30 seconds
- Trade-off: More frequent = less data loss, more disk I/O
- Recommended: 30-60 seconds for spinning disks, 10-30 seconds for SSDs

**Dirty Flag Optimization:**
- Skips idle players (no disk write)
- Reduces I/O by ~50-70% on typical servers

**File Size:**
- Character JSON: ~1-2 KB per file
- 100 players, 30s interval = ~3-6 KB/s write throughput
- Negligible on modern hardware

### Memory Usage

**Per ZonePlayer:**
```
Struct size: ~120 bytes
100 players = ~12 KB
1000 players = ~120 KB
```

Negligible overhead.

### CPU Usage

**Per Auto-Save:**
- Iterate players: O(n)
- JSON serialization: ~0.5ms per character
- File write: ~1ms per character
- 100 players: ~150ms total (once per 30s)

Negligible impact on tick rate.

---

## Future Enhancements

### 1. Zone Config JSON Loading

**TODO:** Load zone config from JSON file:
```json
{
  "zone_id": 10,
  "zone_name": "East Freeport",
  "safe_spawn": {
    "x": 100.0,
    "y": 200.0,
    "z": 10.0,
    "yaw": 90.0
  },
  "autosave_interval_sec": 30.0
}
```

**Implementation:**
```cpp
ZoneConfig loadZoneConfig(const std::string& configPath) {
    // Load JSON
    // Parse fields
    // Return ZoneConfig
}
```

### 2. Graceful Shutdown

**TODO:** Save all player positions on server shutdown:
```cpp
void ZoneServer::shutdown() {
    req::shared::logInfo("zone", "Saving all player positions before shutdown");
    saveAllPlayerPositions();
    ioContext_.stop();
}
```

**Signal Handler:**
```cpp
// Catch SIGINT, SIGTERM
// Call zoneServer.shutdown()
```

### 3. Bind Point Logic

**TODO:** Respawn at bind point on death:
```cpp
if (player.isDead) {
    // Use character.bindX/bindY/bindZ
    player.posX = character.bindX;
    player.posY = character.bindY;
    player.posZ = character.bindZ;
}
```

### 4. Position History

**TODO:** Store last N positions for rollback:
```json
{
  "position_history": [
    {"timestamp": "2025-01-15T12:30:00Z", "x": 100.0, "y": 200.0, "z": 10.0},
    {"timestamp": "2025-01-15T12:25:00Z", "x": 90.0, "y": 190.0, "z": 10.0}
  ]
}
```

**Use Cases:**
- Rollback after bugs
- Anti-cheat analysis
- Player support ("teleported unexpectedly")

### 5. Zone Transfer

**TODO:** Seamless zone-to-zone transfer:
```cpp
// Save position in current zone
savePlayerPosition(characterId);

// Generate handoff token for new zone
auto handoff = generateZoneHandoff(characterId, targetZoneId);

// Client disconnects from current zone
// Client connects to new zone with handoff
```

### 6. Database Integration

**TODO:** Replace JSON files with database:
```cpp
class CharacterStore {
    // PostgreSQL, MySQL, or MongoDB
    void saveCharacter(const Character& char);
    optional<Character> loadById(uint64_t id);
};
```

**Benefits:**
- Atomic updates
- Transaction support
- Better concurrency
- Faster lookups

### 7. Compression

**TODO:** Compress JSON for disk space:
```cpp
// Before write:
std::string json = character.toJson();
std::vector<uint8_t> compressed = gzip::compress(json);
writeFile(compressed);

// On read:
auto compressed = readFile();
std::string json = gzip::decompress(compressed);
Character character = Character::fromJson(json);
```

---

## Troubleshooting

### Problem: Players Always Spawn at Safe Point

**Symptoms:**
- Even returning players spawn at (0, 0, 0)
- No "Restored position" logs

**Possible Causes:**
1. Character JSON not saving correctly
2. `lastZoneId` not matching current zone
3. Saved position is all zeros

**Debug Steps:**
1. Check character JSON file on disk
2. Verify `lastZoneId` matches zone entered
3. Check if `position_x/y/z` are non-zero
4. Enable verbose logging in spawn logic

**Fix:**
```cpp
// Add logging:
req::shared::logInfo("zone", std::string{"[DEBUG] lastZoneId="} + std::to_string(character.lastZoneId) +
    ", currentZoneId=" + std::to_string(zoneId_) +
    ", posX=" + std::to_string(character.positionX));
```

### Problem: Position Not Saving

**Symptoms:**
- No `[SAVE]` logs
- Character JSON unchanged after movement

**Possible Causes:**
1. Player not marked dirty
2. Auto-save interval too long
3. Character store save failing

**Debug Steps:**
1. Check if `player.isDirty` is set
2. Verify auto-save timer is running
3. Check character store logs for errors

**Fix:**
```cpp
// Force save for testing:
player.isDirty = true;
savePlayerPosition(characterId);
```

### Problem: Autosave Too Frequent

**Symptoms:**
- Disk thrashing
- High I/O usage

**Solution:**
```cpp
// Increase interval:
zoneConfig_.autosaveIntervalSec = 60.0f;  // 60 seconds
```

### Problem: Position Lost on Crash

**Symptoms:**
- Server crash ? player position reverted

**Expected Behavior:**
- Normal - positions only saved every 30s and on disconnect

**Mitigations:**
1. Reduce autosave interval (trade-off: more disk I/O)
2. Implement graceful shutdown handler
3. Use database with write-ahead logging

---

## API Reference

### ZoneServer Methods

```cpp
// Set zone configuration
void setZoneConfig(const ZoneConfig& config);

// Save specific player position
void savePlayerPosition(uint64_t characterId);

// Save all dirty player positions
void saveAllPlayerPositions();

// Remove player and save position
void removePlayer(uint64_t characterId);
```

### ZonePlayer Fields

```cpp
struct ZonePlayer {
    uint64_t characterId;
    uint64_t accountId;
    
    float posX, posY, posZ;  // Current position
    float yawDegrees;        // Facing direction
    
    bool isInitialized;      // Player is active
    bool isDirty;            // Position changed since last save
};
```

### ZoneConfig Fields

```cpp
struct ZoneConfig {
    uint32_t zoneId;
    string zoneName;
    
    float safeX, safeY, safeZ, safeYaw;  // Safe spawn point
    float autosaveIntervalSec;            // Auto-save frequency
};
```

---

## Summary

? **Zone Entry:** Smart spawn logic (restore vs safe point)  
? **Position Persistence:** Auto-save every 30s + disconnect save  
? **Character Integration:** Full JSON load/save  
? **Performance:** Dirty flag optimization, minimal I/O  
? **Logging:** Comprehensive spawn and save logs  
? **Backward Compatible:** Handles old characters gracefully  

**Next Steps:**
- Load zone config from JSON
- Implement graceful shutdown
- Add database support
- Implement bind point respawn logic

The character position persistence system now provides a solid foundation for MMO-style gameplay with minimal data loss and intelligent spawn behavior!
