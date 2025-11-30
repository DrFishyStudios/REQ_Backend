# ZoneServer Movement Debugging Guide

## Overview

Comprehensive logging has been added to the ZoneServer movement pipeline to diagnose why positions aren't updating in snapshots. This guide explains what logs to look for and how to interpret them.

---

## Movement Pipeline

The movement system works in three stages:

### Stage 1: MovementIntent Reception
**When:** Client sends MovementIntent (MessageType 40)  
**What happens:** ZoneServer receives and parses client input

### Stage 2: Physics Simulation
**When:** Every 50ms (20 Hz tick rate)  
**What happens:** Server integrates input into velocity and position

### Stage 3: Snapshot Broadcast
**When:** Every tick after simulation  
**What happens:** Server sends updated positions to all clients

---

## Expected Log Flow

### 1. MovementIntent Logs

**When client presses WASD, you should see:**

```
[INFO] [zone] [Movement] Raw payload: '8|559|0.000000|1.000000|0.000000|0|17264544641'
[INFO] [zone] [Movement] Parsed Intent: charId=8, seq=559, input=(0,1), yaw=0, jump=0, clientTimeMs=17264544641
[INFO] [zone] [Movement] Stored input for charId=8: input=(0,1), yaw=0, currentPos=(0,0,0)
```

**What to check:**
- ? **Raw payload** - Should contain 7 pipe-separated fields
- ? **Parsed Intent** - `inputX` and `inputY` should be non-zero when pressing WASD
  - W: `input=(0,1)` - forward
  - A: `input=(-1,0)` - left
  - S: `input=(0,-1)` - backward
  - D: `input=(1,0)` - right
- ? **Stored input** - Should match parsed values
- ? **currentPos** - Shows where player is before movement applied

**If inputX and inputY are always (0,0):**
- ? Unreal client not sending input correctly
- Check UE's `BuildMovementIntentPayload` function
- Verify WASD input is being read

---

### 2. Simulation Logs

**Every second (20 ticks), you should see:**

```
[INFO] [zone] [Sim] Player 8 BEFORE: pos=(0,0,0), input=(0,1)
[INFO] [zone] [Sim] Player 8 velocity: vel=(0,7) from input=(0,1)
[INFO] [zone] [Sim] Player 8 AFTER: pos=(0,0.35,0), moved=0.35 units
```

**What to check:**
- ? **BEFORE pos** - Starting position
- ? **input** - Should match MovementIntent
- ? **velocity** - Should be `input * MAX_SPEED` (7.0 units/sec)
  - Forward: `vel=(0,7)`
  - Left: `vel=(-7,0)`
  - Backward: `vel=(0,-7)`
  - Right: `vel=(7,0)`
- ? **AFTER pos** - Should be `BEFORE + velocity * dt`
  - Delta should be ~0.35 units per tick (7.0 * 0.05)
- ? **moved** - Distance changed (should be > 0 when moving)

**If velocity is (0,0):**
- ? Input not being stored properly
- Check MovementIntent handler stored the values

**If AFTER pos equals BEFORE pos:**
- ? Position integration not working
- Check `player.posX += player.velX * dt` code

---

### 3. Snapshot Logs

**Every second, you should see:**

```
[INFO] [zone] [Snapshot] Building snapshot 21 for 1 active player(s)
[INFO] [zone] [Snapshot] Adding entry: charId=8, pos=(0,7,0), vel=(0,7,0)
[INFO] [zone] [Snapshot] Payload: '21|1|8,0,7,0,0,7,0,0'
[INFO] [zone] [Snapshot] Broadcast snapshot 21 with 1 player(s) to 1 connection(s) [FULL BROADCAST]
```

**What to check:**
- ? **Adding entry pos** - Should match simulation AFTER pos
- ? **Payload** - Wire format: `snapshotId|playerCount|charId,posX,posY,posZ,velX,velY,velZ,yaw`
  - Example: `21|1|8,0,7,0,0,7,0,0`
  - Should show **changing position** (posY increasing when moving forward)
- ? **Broadcast** - Should send to at least 1 connection

**If pos in payload is always (0,0,0):**
- ? Position not being updated in simulation
- Go back and check simulation logs

**If payload shows changing position but UE pawn doesn't move:**
- ? Server is working correctly!
- ? Problem is in Unreal client
- Check UE's snapshot handler and pawn movement

---

## Diagnostic Scenarios

### Scenario A: Input Always Zero

**Symptoms:**
```
[Movement] Parsed Intent: charId=8, seq=559, input=(0,0), yaw=0, jump=0
```

**Diagnosis:** Unreal client not reading input  
**Fix Location:** UE's MovementIntent building code

**Check:**
1. Is UE reading PlayerController input?
2. Is `GetInputAxisValue` working?
3. Are enhanced input mappings correct?

---

### Scenario B: Input Received But No Velocity

**Symptoms:**
```
[Movement] Stored input for charId=8: input=(0,1)
[Sim] Player 8 BEFORE: pos=(0,0,0), input=(0,1)
[Sim] Player 8 velocity: vel=(0,0)  ? Should be (0,7)!
```

**Diagnosis:** Velocity computation broken  
**Fix Location:** ZoneServer `updateSimulation()`

**Check:**
```cpp
player.velX = dirX * MAX_SPEED;  // Should compute (0 * 7 = 0) or (1 * 7 = 7)
player.velY = dirY * MAX_SPEED;
```

---

### Scenario C: Velocity Computed But Position Not Changing

**Symptoms:**
```
[Sim] Player 8 velocity: vel=(0,7)
[Sim] Player 8 AFTER: pos=(0,0,0), moved=0 units  ? Should be (0,0.35,0)!
```

**Diagnosis:** Position integration broken  
**Fix Location:** ZoneServer `updateSimulation()`

**Check:**
```cpp
float newPosX = player.posX + player.velX * dt;  // dt should be 0.05
float newPosY = player.posY + player.velY * dt;
float newPosZ = player.posZ + player.velZ * dt;

// ...then later...
player.posX = newPosX;  // Make sure this assignment happens!
player.posY = newPosY;
player.posZ = newPosZ;
```

---

### Scenario D: Position Updates But Snapshot Has Zeros

**Symptoms:**
```
[Sim] Player 8 AFTER: pos=(0,7,0)
[Snapshot] Adding entry: charId=8, pos=(0,0,0)  ? Should be (0,7,0)!
```

**Diagnosis:** Snapshot using wrong data  
**Fix Location:** ZoneServer `broadcastSnapshots()`

**Check:**
```cpp
entry.posX = player.posX;  // Make sure reading from correct player object
entry.posY = player.posY;
entry.posZ = player.posZ;
```

---

### Scenario E: Everything Server-Side Correct, UE Pawn Doesn't Move

**Symptoms:**
```
[Snapshot] Payload: '21|1|8,0,7,0,0,7,0,0'  ? Position changing! ?
```
**But UE pawn stays at (0,0,0)**

**Diagnosis:** Unreal client not applying snapshot  
**Fix Location:** UE's OnPlayerStateSnapshot handler

**Check:**
1. Is `OnPlayerStateSnapshot` delegate bound?
2. Is `ParsePlayerStateSnapshotPayload` working?
3. Is `OnLocalPlayerStateUpdated` firing?
4. Is pawn's `Tick` applying the position?

---

## Movement Constants

**Defined in ZoneServer.cpp:**

```cpp
constexpr float TICK_RATE_HZ = 20.0f;         // 20 ticks per second
constexpr float TICK_DT = 0.05f;              // 0.05 seconds per tick
constexpr float MAX_SPEED = 7.0f;             // units per second
constexpr float GRAVITY = -30.0f;             // units per second squared (Z-axis)
constexpr float JUMP_VELOCITY = 10.0f;        // initial upward velocity
constexpr float GROUND_LEVEL = 0.0f;          // Z=0 is ground
```

**Expected Movement Per Tick:**
- Forward (input Y=1): `velY = 7.0`, deltaY per tick = `7.0 * 0.05 = 0.35 units`
- Left (input X=-1): `velX = -7.0`, deltaX per tick = `-7.0 * 0.05 = -0.35 units`

**After 20 ticks (1 second):**
- Forward: traveled `7.0 units` on Y-axis
- Left: traveled `-7.0 units` on X-axis

---

## Testing Steps

### Step 1: Start Servers
```
LoginServer (port 7777)
WorldServer (port 7778)
ZoneServer (port 7780)
```

### Step 2: Connect UE Client
- Login ? Enter World ? Enter Zone
- Should see spawn logs

### Step 3: Press W and Hold

**Watch ZoneServer console for:**

1. **MovementIntent logs (immediate):**
   ```
   [Movement] Parsed Intent: input=(0,1)
   ```

2. **Simulation logs (every 1 second):**
   ```
   [Sim] Player 8 velocity: vel=(0,7)
   [Sim] Player 8 AFTER: pos=(0,0.35,0)
   ```

3. **Snapshot logs (every 1 second):**
   ```
   [Snapshot] Payload: '22|1|8,0,0.35,0,0,7,0,0'
   [Snapshot] Payload: '42|1|8,0,7,0,0,7,0,0'    ? Position increasing!
   [Snapshot] Payload: '62|1|8,0,14,0,0,7,0,0'
   ```

**Expected Results:**
- ? Input shows `(0,1)` - forward
- ? Velocity shows `(0,7)` - 7 units/sec forward
- ? Position increases by ~0.35 per tick
- ? After 1 second, posY should be ~7.0
- ? Snapshot payload shows increasing posY

---

## Troubleshooting Quick Reference

| Symptom | Problem Location | What to Check |
|---------|------------------|---------------|
| Input always (0,0) | UE Client | Input reading, BuildMovementIntent |
| Input received, velocity=0 | ZoneServer simulation | Velocity computation from input |
| Velocity computed, pos not changing | ZoneServer simulation | Position integration (pos += vel * dt) |
| Pos changes in sim, zeros in snapshot | ZoneServer snapshot | Snapshot reading correct player data |
| Snapshot correct, UE pawn doesn't move | UE Client | Snapshot parsing, pawn movement |

---

## Log Volume Control

**Detailed logs appear every 1 second (20 ticks)**

To reduce log spam, adjust counters in code:

```cpp
// In updateSimulation()
bool doDetailedLog = (++simLogCounter % 20 == 0);  // Change 20 to 100 for every 5 seconds

// In broadcastSnapshots()
bool doDetailedLog = (++logCounter % 20 == 0);  // Change 20 to 100 for every 5 seconds
```

---

## Summary

**The movement system is already implemented and should work!**

The code in `updateSimulation()` correctly:
1. ? Reads input from MovementIntent
2. ? Computes velocity from input
3. ? Integrates position over time
4. ? Applies gravity and jump
5. ? Checks for suspicious movement (anti-cheat)

**The logs will tell you exactly where the pipeline breaks:**
- If input is zero ? UE client issue
- If velocity is zero ? simulation velocity computation
- If position not changing ? simulation integration
- If snapshot has zeros ? snapshot building
- If snapshot correct but pawn doesn't move ? UE client issue

**Next steps:**
1. Run with these logs enabled
2. Press W in UE client
3. Watch the ZoneServer console
4. Find which stage shows zeros when it shouldn't
5. Fix that specific stage

**The logs are your diagnostic tool!** ??
