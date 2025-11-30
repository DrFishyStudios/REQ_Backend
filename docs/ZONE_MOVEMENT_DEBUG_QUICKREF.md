# Movement Debugging - Quick Reference

## TL;DR

Added comprehensive logging to track why positions aren't updating. The movement system is **already implemented** - logs will show where it's breaking.

---

## What Was Added

### 1. MovementIntent Logging
```
[Movement] Raw payload: '8|559|0.000000|1.000000|0.000000|0|17264544641'
[Movement] Parsed Intent: charId=8, seq=559, input=(0,1), yaw=0, jump=0
[Movement] Stored input for charId=8: input=(0,1), currentPos=(0,0,0)
```

### 2. Simulation Logging (every 1 sec)
```
[Sim] Player 8 BEFORE: pos=(0,0,0), input=(0,1)
[Sim] Player 8 velocity: vel=(0,7) from input=(0,1)
[Sim] Player 8 AFTER: pos=(0,0.35,0), moved=0.35 units
```

### 3. Snapshot Logging (every 1 sec)
```
[Snapshot] Adding entry: charId=8, pos=(0,7,0), vel=(0,7,0)
[Snapshot] Payload: '21|1|8,0,7,0,0,7,0,0'
[Snapshot] Broadcast snapshot 21 with 1 player(s) to 1 connection(s)
```

---

## Expected When Pressing W

### MovementIntent (immediate)
```
input=(0,1)  ? Y=1 means forward
```

### Simulation (every 1 sec)
```
velocity: vel=(0,7)              ? 7 units/sec forward
AFTER: pos=(0,0.35,0)            ? 0.35 units per tick
AFTER: pos=(0,7,0)               ? 7 units after 1 second
AFTER: pos=(0,14,0)              ? 14 units after 2 seconds
```

### Snapshot (every 1 sec)
```
Payload: '21|1|8,0,0.35,0,0,7,0,0'   ? posY=0.35
Payload: '41|1|8,0,7,0,0,7,0,0'      ? posY=7.0
Payload: '61|1|8,0,14,0,0,7,0,0'     ? posY=14.0
```

**If posY stays at 0 ? movement system broken**  
**If posY increases ? movement system working, UE client issue**

---

## Diagnostic Quick Check

| Log Shows | Means |
|-----------|-------|
| `input=(0,0)` always | UE not sending input |
| `input=(0,1)` but `vel=(0,0)` | Velocity computation broken |
| `vel=(0,7)` but `pos=(0,0,0)` stays same | Integration broken |
| `pos=(0,7,0)` but snapshot has `0,0,0,0` | Snapshot building broken |
| Snapshot has `0,7,0` but UE pawn doesn't move | UE client issue |

---

## Movement Constants

```
TICK_RATE: 20 Hz (every 50ms)
MAX_SPEED: 7.0 units/sec
Movement per tick: 0.35 units (7.0 * 0.05)
```

---

## Files Modified

? `REQ_ZoneServer/src/ZoneServer.cpp`
- Added logging to MovementIntent handler
- Added logging to updateSimulation()
- Added logging to broadcastSnapshots()

---

## Testing

1. Start all servers
2. Connect UE client
3. Press W and watch ZoneServer console
4. Find where values become zero

**The logs will tell you exactly what's broken!** ??
