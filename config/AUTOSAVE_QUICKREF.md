# Character Position Autosave - Quick Reference

## Overview
ZoneServer automatically saves character positions at regular intervals and on disconnect to ensure data persistence.

---

## Features

### 1. Periodic Autosave
- **Interval:** 30 seconds (configurable via `ZoneConfig.autosaveIntervalSec`)
- **Scope:** All dirty (modified) characters in the zone
- **Optimization:** Only saves characters that have moved since last save

### 2. Disconnect Save
- **Trigger:** When player disconnects from zone
- **Guarantee:** Final position always saved before character removed
- **Implementation:** `ZoneServer::removePlayer()` calls `savePlayerPosition()`

### 3. Dirty Flag Optimization
- **Mechanism:** `ZonePlayer::isDirty` flag tracks if position changed
- **Benefit:** Skips unnecessary file writes for stationary players
- **Reset:** Flag cleared after successful save

---

## Configuration

### Zone Config (ZoneServer)
```cpp
ZoneConfig config;
config.autosaveIntervalSec = 30.0f;  // 30 seconds (default)
```

**To Change Interval:**
1. Modify `ZoneConfig.autosaveIntervalSec` when initializing `ZoneServer`
2. Or: Load from zone configuration JSON (future enhancement)

**Recommended Values:**
- **High-traffic zones:** 15-20 seconds (more frequent saves)
- **Low-traffic zones:** 30-60 seconds (reduce disk I/O)
- **Development:** 5-10 seconds (for testing)

---

## Log Output

### Autosave Tick
```
[INFO] [zone] [AUTOSAVE] Saved positions for 3 player(s), skipped 2
```

**Interpretation:**
- **Saved:** 3 characters had position changes (dirty flag set)
- **Skipped:** 2 characters haven't moved since last save

### Individual Save
```
[INFO] [zone] [SAVE] Position saved: characterId=1, zoneId=10, pos=(100.5,200.0,10.0), yaw=90.0
```

**Fields:**
- `characterId`: Character being saved
- `zoneId`: Current zone ID
- `pos`: Current position (x, y, z)
- `yaw`: Facing direction in degrees

### Disconnect Save
```
[INFO] [zone] [DISCONNECT] Saving final position for characterId=1
[INFO] [zone] [SAVE] Position saved: characterId=1, zoneId=10, pos=(150.0,250.0,15.0), yaw=180.0
[INFO] [zone] [DISCONNECT] Player removed: characterId=1, remaining players=2
```

### Save Error
```
[ERROR] [zone] [SAVE] Failed to save character position: characterId=1
```

**Causes:**
- File permissions issue
- Disk full
- JSON serialization error
- Character file corrupted

**Recovery:** Position lost this save cycle, will retry on next autosave or disconnect

---

## Data Flow

### 1. Character Movement
```
Client sends MovementIntent
    ?
ZoneServer::updateSimulation()
    ?
Position updated: ZonePlayer.posX/Y/Z
    ?
Dirty flag set: ZonePlayer.isDirty = true
```

### 2. Autosave Tick
```
30 seconds elapse
    ?
ZoneServer::onAutosave()
    ?
ZoneServer::saveAllPlayerPositions()
    ?
For each dirty player:
    - Load character from CharacterStore
    - Update lastWorldId, lastZoneId, position, heading
    - Save character JSON to disk
    - Clear isDirty flag
```

### 3. Disconnect Save
```
Client disconnects
    ?
ZoneServer::onConnectionClosed()
    ?
ZoneServer::removePlayer(characterId)
    ?
ZoneServer::savePlayerPosition(characterId)
    - Load character
    - Update position (ignores dirty flag)
    - Save to disk
    ?
Remove from players map
```

---

## Character JSON Updates

### Fields Updated on Save
```json
{
  "character_id": 1,
  "last_world_id": 1,       // ? Updated
  "last_zone_id": 10,        // ? Updated
  "position_x": 100.5,       // ? Updated
  "position_y": 200.0,       // ? Updated
  "position_z": 10.0,        // ? Updated
  "heading": 90.0            // ? Updated
}
```

**Note:** Only world/zone tracking and position fields are updated. Stats, inventory, level, etc. remain unchanged.

---

## Testing Autosave

### Manual Test Procedure

**Step 1: Start servers**
```bash
cd x64/Debug
.\REQ_LoginServer.exe
.\REQ_WorldServer.exe
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="Test Zone"
```

**Step 2: Connect and move**
```bash
.\REQ_TestClient.exe
# Login, create character, enter zone
# Move with commands: w, a, s, d
```

**Step 3: Monitor ZoneServer logs**
```
[INFO] [zone] Position autosave enabled: interval=30s
[INFO] [zone] [AUTOSAVE] Saved positions for 1 player(s), skipped 0
[INFO] [zone] [SAVE] Position saved: characterId=1, zoneId=10, pos=(35.0,0,0), yaw=0.0
```

**Step 4: Verify save file**
```bash
# Check character JSON
cat data/characters/1.json
# Verify position_x/y/z match logged values
```

**Step 5: Test disconnect save**
```
# In TestClient, enter: q
[INFO] [zone] [DISCONNECT] Saving final position for characterId=1
[INFO] [zone] [SAVE] Position saved: characterId=1, zoneId=10, pos=(50.0,0,0), yaw=0.0
```

**Step 6: Reconnect and verify position**
```bash
.\REQ_TestClient.exe
# Login with same character
# Zone logs should show:
[INFO] [zone] [SPAWN] Restored position for characterId=1: pos=(50.0,0,0), yaw=0.0
```

---

## Troubleshooting

### Autosave Not Happening
**Symptoms:** No `[AUTOSAVE]` logs every 30 seconds

**Causes:**
- Autosave timer not scheduled (check `scheduleAutosave()` called in `run()`)
- Timer interval misconfigured
- Server event loop blocked

**Fix:**
- Verify ZoneServer logs show "Position autosave enabled: interval=30s" on startup
- Check no blocking operations in message handlers
- Restart ZoneServer

### Position Not Updating
**Symptoms:** Character reconnects at old position

**Causes:**
- Dirty flag not set during movement
- Save operation failing silently
- Character file not writable

**Fix:**
- Check for `[ERROR] [zone] [SAVE]` logs
- Verify `data/characters/` directory permissions
- Check disk space

### Disconnect Save Not Working
**Symptoms:** Position lost on disconnect

**Causes:**
- `onConnectionClosed()` not being called
- `removePlayer()` not called
- Save operation failing

**Fix:**
- Check for `[DISCONNECT]` logs when client exits
- Verify connection cleanup logic
- Add connection close detection if missing

---

## Performance Considerations

### Disk I/O Impact
- **Per Character:** ~1KB JSON file write
- **10 Players:** ~10KB every 30 seconds = ~20KB/minute
- **100 Players:** ~100KB every 30 seconds = ~200KB/minute

**Optimization:**
- Dirty flag reduces writes by ~50-70% (many players don't move every 30s)
- JSON is lightweight (future: consider binary format)

### CPU Impact
- JSON serialization: ~0.1ms per character
- File write: ~1-5ms per character (depends on disk)
- Total per autosave: ~10-50ms for 10 players

**Scaling:**
- Up to ~100 concurrent players per zone: negligible impact
- 1000+ players: consider batching or async writes

---

## Code References

### Key Files
- **ZoneServer.h**: `ZonePlayer` struct, `savePlayerPosition()`, autosave methods
- **ZoneServer.cpp**: Implementation of autosave loop and disconnect save
- **CharacterStore.h/cpp**: `saveCharacter()` method

### Key Methods
```cpp
// ZoneServer.cpp
void ZoneServer::scheduleAutosave();      // Start autosave timer
void ZoneServer::onAutosave(...);          // Autosave tick handler
void ZoneServer::saveAllPlayerPositions(); // Save all dirty players
void ZoneServer::savePlayerPosition(...);  // Save single player
void ZoneServer::removePlayer(...);        // Disconnect + save
```

### Key Structs
```cpp
struct ZonePlayer {
    bool isDirty;  // Position changed since last save
    float posX, posY, posZ;
    float yawDegrees;
    // ...
};

struct ZoneConfig {
    float autosaveIntervalSec;  // Default: 30.0f
};
```

---

## Future Enhancements

### Planned Features
- [ ] Configurable autosave interval via zone JSON config
- [ ] Async file writes (non-blocking I/O)
- [ ] Batch save optimization (group writes)
- [ ] Position history / rollback capability
- [ ] Save validation (checksum/integrity checks)

### Planned Metrics
- [ ] Save success/failure counters
- [ ] Average save time per character
- [ ] Dirty vs clean player ratios
- [ ] Disk space usage tracking

---

## Quick Commands

### Check Recent Autosaves (Linux/macOS)
```bash
ls -lht data/characters/*.json | head -n 10
```

### Monitor Autosave Logs
```bash
tail -f zone_server.log | grep AUTOSAVE
```

### Inspect Character Position
```bash
cat data/characters/1.json | jq '.position_x, .position_y, .position_z, .heading'
```

### Compare Before/After Save
```bash
# Before movement
cat data/characters/1.json > before.json

# Move character, wait for autosave...

# After movement
cat data/characters/1.json > after.json

# Compare
diff before.json after.json
```

---

## Summary

**Autosave ensures:**
? Periodic saves every 30 seconds (configurable)
? Guaranteed save on disconnect
? Optimized with dirty flag (only save what changed)
? Comprehensive logging for debugging
? Resilient error handling (failures don't crash server)

**CLI inspection:**
```
> show_char 1
Position:         (100.5, 200.0, 10.0)   # ? Verify position saved
Heading:          90 degrees
```

**All backend-only - no Unreal changes required! ??**
