# Multi-Player Quick Reference

## Data Structures

```cpp
// Primary player map (key = characterId)
std::unordered_map<uint64_t, ZonePlayer> players_;

// Reverse lookup (connection ? characterId)
std::unordered_map<ConnectionPtr, uint64_t> connectionToCharacterId_;

// All active connections
std::vector<ConnectionPtr> connections_;
```

## Lifecycle Events

### Player Joins Zone
```
[ZonePlayer created] characterId=X, accountId=Y, zoneId=Z, 
                     pos=(x,y,z), yaw=degrees, active_players=N
```

### Player Leaves Zone
```
[Connection] Found characterId=X for closed connection
[DISCONNECT] Saving final position for characterId=X
[SAVE] Position saved: characterId=X, zoneId=Z, pos=(x,y,z), yaw=degrees
[ZonePlayer removed] characterId=X, reason=disconnect, remaining_players=N
```

### Snapshot Broadcast (Every ~5 seconds)
```
[Snapshot] Building snapshot N for X active player(s)
[Snapshot] Broadcast snapshot N with X player(s) to Y connection(s)
```

## Key Methods

| Method | Purpose | When Called |
|--------|---------|-------------|
| `handleMessage(ZoneAuthRequest)` | Create ZonePlayer | Client authenticates |
| `onConnectionClosed(conn)` | Find and remove player | Connection drops |
| `removePlayer(characterId)` | Save and cleanup | Disconnect or duplicate |
| `broadcastSnapshots()` | Send state to all | Every tick (20Hz) |

## Testing Commands

### Start Servers
```bash
cd x64/Debug
.\REQ_LoginServer.exe
.\REQ_WorldServer.exe
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="Test"
```

### Connect Multiple Clients
```bash
# Terminal 1
.\REQ_TestClient.exe

# Terminal 2  
.\REQ_TestClient.exe
```

### Expected: Both clients see BOTH players in snapshots

## Debug Tips

### Enable Detailed Snapshot Logging
In `ZoneServer.cpp`, uncomment block in `broadcastSnapshots()`:
```cpp
/*
if (logCounter % 100 == 0) {
    req::shared::logInfo("zone", std::string{"[Snapshot] Snapshot "} + ...);
    for (const auto& entry : snapshot.players) {
        req::shared::logInfo("zone", std::string{"  characterId="} + ...);
    }
}
*/
```

### Check Player Count
```cpp
[ZonePlayer created] ... active_players=N  ? Should increment
[ZonePlayer removed] ... remaining_players=N  ? Should decrement
[Snapshot] ... with N player(s) to M connection(s)  ? N should match active count
```

### Verify Multi-Player
- Start 2 TestClients
- Both should see `[Snapshot] ... with 2 player(s)`
- Move in one client ? other client sees movement in snapshots
- Disconnect one ? other sees `with 1 player(s)`

## Common Issues

| Issue | Symptom | Fix |
|-------|---------|-----|
| Only see self | Snapshots have 1 player with 2 connected | Check `broadcastSnapshots()` iterates all `players_` |
| Stale players | Disconnected player still in snapshots | Verify `onConnectionClosed()` is called |
| Duplicate logins | Two entries for same characterId | Check duplicate check in ZoneAuth handler |
| No disconnect save | Position lost on disconnect | Verify `removePlayer()` calls `savePlayerPosition()` |

## Architecture at a Glance

```
???????????????
?  Connection ? ????
???????????????    ?
                   ???? ZoneAuthRequest
???????????????    ?
?  Connection ? ????
???????????????

        ? handleMessage()
        
????????????????????????????
?  players_ map            ?
?  ??????????????????????  ?
?  ? characterId ? ?    ?  ?
?  ?   ZonePlayer  ?    ?  ?
?  ?   - connection????????? ? Send snapshots
?  ?   - position  ?    ?  ?
?  ?   - velocity  ?    ?  ?
?  ??????????????????????  ?
????????????????????????????

        ? updateSimulation() (20Hz)
        
????????????????????????????
?  Snapshot Builder        ?
?  ??????????????????????  ?
?  ? For each player:   ?  ?
?  ?   - characterId    ?  ?
?  ?   - pos, vel, yaw  ?  ?
?  ??????????????????????  ?
????????????????????????????

        ? broadcastSnapshots()
        
???????????????    ???????????????
?  Client A   ?    ?  Client B   ?
?  Receives:  ?    ?  Receives:  ?
?  - Player A ?    ?  - Player A ?
?  - Player B ?    ?  - Player B ?
???????????????    ???????????????
```

## Quick Checklist

- [x] ZonePlayer keyed by characterId ?
- [x] Connection pointer stored in ZonePlayer ?
- [x] Lifecycle logging (create/remove) ?
- [x] Disconnect handler wired up ?
- [x] Duplicate login protection ?
- [x] Snapshots include all players ?
- [x] Broadcast to all connections ?
- [x] Proper cleanup on disconnect ?

**Multi-player architecture is complete and robust! ??**
