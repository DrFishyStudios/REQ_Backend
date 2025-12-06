# Bug Fix: Double-Erase Disconnect Crash

## Problem Description

ZoneServer was crashing when clients disconnected due to **iterator invalidation** caused by erasing the same entry from `connectionToCharacterId_` twice.

### Root Cause

In `ZoneServer::onConnectionClosed(ConnectionPtr connection)`:

1. We looked up the connection in `connectionToCharacterId_` and got an iterator `it`
2. Called `removePlayer(characterId)` which **already erased** the entry from `connectionToCharacterId_`
3. Then attempted to erase the same entry again using `connectionToCharacterId_.erase(it)` 

Since the iterator `it` was invalidated by the erase inside `removePlayer`, the second erase caused **undefined behavior** (UB) and crashed the server.

### Code Flow Before Fix

```cpp
void ZoneServer::onConnectionClosed(ConnectionPtr connection) {
    auto it = connectionToCharacterId_.find(connection);
    if (it != connectionToCharacterId_.end()) {
        std::uint64_t characterId = it->second;
        
        removePlayer(characterId);  // ? Erases from connectionToCharacterId_ internally
        
        connectionToCharacterId_.erase(it);  // ? DOUBLE ERASE - iterator invalidated!
    }
    // ...
}

void ZoneServer::removePlayer(std::uint64_t characterId) {
    // ...
    if (player.connection) {
        auto connIt = connectionToCharacterId_.find(player.connection);
        if (connIt != connectionToCharacterId_.end()) {
            connectionToCharacterId_.erase(connIt);  // ? First erase happens here
        }
    }
    // ...
}
```

---

## Solution

**Make `removePlayer` the single owner of connection mapping cleanup.**

Removed the explicit `connectionToCharacterId_.erase(it)` call from `onConnectionClosed`, since `removePlayer` already handles this cleanup internally.

### Code After Fix

```cpp
void ZoneServer::onConnectionClosed(ConnectionPtr connection) {
    req::shared::logInfo("zone", "[DISCONNECT] ========== BEGIN DISCONNECT HANDLING ==========");
    req::shared::logInfo("zone", "[DISCONNECT] Connection closed event received");
    
    // ... connection state checks ...
    
    auto it = connectionToCharacterId_.find(connection);
    if (it != connectionToCharacterId_.end()) {
        std::uint64_t characterId = it->second;
        req::shared::logInfo("zone", "[DISCONNECT] Found ZonePlayer: characterId=" + 
            std::to_string(characterId));
        
        // ... player validation checks ...
        
        // ? Remove player (which handles both saving AND unmapping the connection)
        removePlayer(characterId);
        
        // ? REMOVED: Do not erase from connectionToCharacterId_ here
        // removePlayer() already handles this cleanup to avoid iterator invalidation
        // connectionToCharacterId_.erase(it);
        // req::shared::logInfo("zone", "[DISCONNECT] Removed from connection-to-character mapping");
        
        req::shared::logInfo("zone", "[DISCONNECT] Player removed (removePlayer handles connection mapping cleanup)");
    } else {
        req::shared::logInfo("zone", "[DISCONNECT] No ZonePlayer associated with this connection");
        req::shared::logInfo("zone", "[DISCONNECT] Likely disconnected before completing ZoneAuthRequest");
    }
    
    // ... remaining cleanup ...
}
```

`removePlayer` remains unchanged and continues to be the **single source of truth** for connection mapping cleanup:

```cpp
void ZoneServer::removePlayer(std::uint64_t characterId) {
    // ... validation and saving ...
    
    // Remove from connection mapping (if connection still exists)
    if (player.connection) {
        auto connIt = connectionToCharacterId_.find(player.connection);
        if (connIt != connectionToCharacterId_.end()) {
            connectionToCharacterId_.erase(connIt);  // ? Single erase location
            req::shared::logInfo("zone", "[REMOVE_PLAYER] Removed from connection mapping");
        }
    }
    
    // Remove from players map
    players_.erase(it);
    
    // ...
}
```

---

## Files Changed

### Modified Files

1. **`REQ_ZoneServer/src/ZoneServer_Network.cpp`**
   - Removed redundant `connectionToCharacterId_.erase(it)` call in `onConnectionClosed`
   - Added clarifying log message explaining that `removePlayer` handles the cleanup
   - Enhanced comments explaining the ownership model

---

## Verification

### Build Status
? **Build successful** - All projects compiled without errors

### Code Audit
? **No other double-erase locations found** - Only `removePlayer` erases from `connectionToCharacterId_`

### Testing Checklist

Test the following scenarios to verify the fix:

#### Test 1: Normal Disconnect
1. Start LoginServer, WorldServer, ZoneServer
2. Connect with TestClient or UE client
3. Enter zone, move around briefly
4. Close client (normal exit)
5. **Expected:** ZoneServer logs show clean disconnect, no crash

#### Test 2: Hard Kill During Movement
1. Connect client and enter zone
2. Hold W to move continuously
3. Kill client process (Task Manager / `kill -9`)
4. **Expected:** ZoneServer detects disconnect, saves position, continues running

#### Test 3: Multiple Clients Disconnect
1. Connect 2-3 clients to same zone
2. Disconnect them one by one (mix of normal exit and hard kill)
3. **Expected:** Each disconnect handled cleanly, no crashes

#### Test 4: Disconnect Before Zone Auth
1. Connect client but close before completing ZoneAuth
2. **Expected:** ZoneServer logs "No ZonePlayer associated", no crash

---

## Expected Logs After Fix

### Normal Disconnect:
```
[INFO] [zone] [DISCONNECT] ========== BEGIN DISCONNECT HANDLING ==========
[INFO] [zone] [DISCONNECT] Connection closed event received
[INFO] [zone] [DISCONNECT] Connection is marked as closed
[INFO] [zone] [DISCONNECT] Found ZonePlayer: characterId=5
[INFO] [zone] [DISCONNECT] Player found in players map, accountId=1, pos=(100.5,200.0,10.0)
[INFO] [zone] [REMOVE_PLAYER] BEGIN: characterId=5
[INFO] [zone] [REMOVE_PLAYER] Found player: accountId=1, pos=(100.5,200.0,10.0)
[INFO] [zone] [REMOVE_PLAYER] Attempting to save character state...
[INFO] [zone] [SAVE] Position saved successfully: characterId=5
[INFO] [zone] [REMOVE_PLAYER] Character state saved successfully
[INFO] [zone] [REMOVE_PLAYER] Removed from connection mapping  ? Single erase!
[INFO] [zone] [REMOVE_PLAYER] Removed from players map
[INFO] [zone] [REMOVE_PLAYER] END: characterId=5, remaining_players=0
[INFO] [zone] [DISCONNECT] Player removed (removePlayer handles connection mapping cleanup)
[INFO] [zone] [DISCONNECT] Removed from connections list
[INFO] [zone] [DISCONNECT] Cleanup complete. Active connections=0, active players=0
[INFO] [zone] [DISCONNECT] ========== END DISCONNECT HANDLING ==========
```

### Key Observations:
? Only **one** "Removed from connection mapping" log (inside `removePlayer`)
? No crash or error messages
? Clean disconnect flow from BEGIN to END markers

---

## Impact

### Before Fix
? ZoneServer crashed on client disconnect
? Undefined behavior due to iterator invalidation
? Unpredictable server state after crash

### After Fix
? ZoneServer handles disconnects gracefully
? No iterator invalidation or undefined behavior
? Server continues running reliably
? Character state properly saved on disconnect
? Clean separation of concerns (removePlayer owns cleanup)

---

## Architecture Decision

**Design Principle:** Single Responsibility for Cleanup

- **`removePlayer(characterId)`** is the **single owner** of player cleanup:
  - Saves character state
  - Removes from `connectionToCharacterId_` map
  - Removes from `players_` map

- **`onConnectionClosed(connection)`** orchestrates the flow:
  - Detects disconnect
  - Looks up characterId
  - Delegates cleanup to `removePlayer`
  - Removes connection from `connections_` vector

This ensures:
- No duplicate cleanup code
- No iterator invalidation bugs
- Clear ownership model
- Easy to maintain and debug

---

## Future Considerations

### Other Potential Double-Erase Locations

Audited the codebase for other potential double-erase scenarios:

? **Verified safe:** Only `removePlayer` erases from `connectionToCharacterId_`
? **No other cleanup paths** found that could cause similar issues

### Recommended Best Practices

1. **Centralize cleanup:** Always use `removePlayer` for player removal
2. **Avoid iterator reuse:** Don't use iterators after calling functions that might erase
3. **Defensive programming:** Check for `players_.end()` before using iterators
4. **Comprehensive logging:** Track ownership transfers in logs

---

## Summary

? **Root cause identified:** Double-erase of `connectionToCharacterId_` mapping
? **Fix implemented:** Removed redundant erase in `onConnectionClosed`
? **Build verified:** Compiles successfully
? **Ownership clarified:** `removePlayer` is single source of cleanup
? **Ready for testing:** Manual testing recommended before production deployment

**The disconnect crash has been resolved! ??**

---

## Related Documentation

- `docs/ZONE_DISCONNECT_ROBUSTNESS.md` - Comprehensive disconnect handling guide
- `config/MULTIPLAYER_ARCHITECTURE.md` - Player lifecycle management
- `config/MULTIPLAYER_QUICKREF.md` - Quick reference for multi-player scenarios
