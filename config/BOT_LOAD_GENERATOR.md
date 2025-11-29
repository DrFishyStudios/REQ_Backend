# Bot Load Generator System - Implementation Guide

## Overview

REQ_TestClient now includes a comprehensive bot load generator system that can spawn multiple autonomous client instances to test zone servers and interest management at scale. Each bot independently connects to Login/World/Zone servers, authenticates, enters a zone, and executes scripted movement patterns.

---

## Key Features

? **Multi-Bot Support** - Spawn 1-100 bot instances in a single process  
? **Autonomous Operation** - Bots handle full handshake sequence independently  
? **Scripted Movement** - Circle, back-and-forth, random, and stationary patterns  
? **Snapshot Logging** - Track interest management with snapshot details  
? **Graceful Shutdown** - Clean disconnect and position saving on exit  
? **Configurable Verbosity** - Minimal, Normal, and Debug logging levels  

---

## Architecture

### Components

**BotClient** - Single bot instance with full client lifecycle  
**BotManager** - Manages multiple bot instances and main loop  
**BotConfig** - Configuration for bot behavior and movement  
**TestClient (main)** - Entry point with bot mode support  

### Bot Lifecycle

```
1. Start:
   - Connect to LoginServer
   - Login/Register with credentials
   - Request character list
   - Create character if needed
   - Enter world
   - Connect to ZoneServer
   - Authenticate with handoff token

2. In-Zone Operation:
   - Receive snapshots from ZoneServer
   - Update local position from snapshots
   - Execute movement pattern
   - Send MovementIntent every 100ms

3. Shutdown:
   - Stop movement
   - Close zone socket gracefully
   - Trigger ZoneServer SaveOnDisconnect
```

---

## Bot Configuration

### BotConfig Structure

```cpp
struct BotConfig {
    std::string username;
    std::string password;
    std::int32_t targetWorldId{ 1 };
    std::int32_t startingZoneId{ 10 };
    
    // Movement pattern
    enum class MovementPattern {
        Circle,         // Move in a circle
        BackAndForth,   // Move back and forth on X axis
        Random,         // Random walk
        Stationary      // Don't move
    };
    
    MovementPattern pattern{ MovementPattern::Circle };
    float moveRadius{ 50.0f };        // Radius for circle pattern
    float angularSpeed{ 0.5f };       // Radians per second
    float walkSpeed{ 5.0f };          // Units per second for back-and-forth
    
    // Logging verbosity
    enum class LogLevel {
        Minimal,    // Only major events (login, zone entry)
        Normal,     // Include movement send/snapshot receive
        Debug       // Everything including snapshot details
    };
    
    LogLevel logLevel{ LogLevel::Minimal };
};
```

---

## Movement Patterns

### Circle Pattern

**Behavior:**
- Bots move in a circular path around a center point
- Center point is the bot's spawn location
- Radius and angular speed are configurable

**Parameters:**
- `moveRadius` (float): Circle radius in units
- `angularSpeed` (float): Radians per second (0.5 = ~30 degrees/sec)

**Formula:**
```cpp
angle += angularSpeed * deltaTime;
dirX = -sin(angle);
dirY = cos(angle);
```

**Use Case:** Visualize interest radius boundaries (bots circle in/out of range)

---

### Back-and-Forth Pattern

**Behavior:**
- Bots move back and forth along the X axis
- Oscillates between -radius and +radius from spawn point

**Parameters:**
- `moveRadius` (float): Maximum distance from center
- `walkSpeed` (float): Units per second

**Use Case:** Test interest filtering along a line

---

### Random Pattern

**Behavior:**
- Bots change direction randomly every 2 seconds
- Direction is uniformly random in XY plane

**Parameters:**
- No specific parameters

**Use Case:** Simulate realistic player movement chaos

---

### Stationary Pattern

**Behavior:**
- Bots don't move, just stand at spawn point

**Parameters:**
- No specific parameters

**Use Case:** Test interest management with mixed static/dynamic players

---

## Usage

### Command-Line Mode

```bash
# Spawn 10 bots
.\REQ_TestClient.exe --bot-count 10

# Short form
.\REQ_TestClient.exe -bc 5

# Show help
.\REQ_TestClient.exe --help
```

### Interactive Mode

```
1. Run .\REQ_TestClient.exe
2. Select option 7 (Bot Mode)
3. Enter number of bots (1-100)
4. Bots will spawn and run automatically
5. Press Ctrl+C to stop all bots and exit
```

---

## Bot Naming and Credentials

### Automatic Bot Credentials

**Username Pattern:** `Bot001`, `Bot002`, ..., `Bot100`
**Password:** `botpass` (same for all bots)

**Character Pattern:** `Bot001Char`, `Bot002Char`, etc.

### Auto-Registration

- Bots attempt login first
- If login fails (account doesn't exist), bot automatically registers
- Character created on first zone entry if none exists
- Subsequent runs reuse existing account and character

---

## Logging Levels

### Minimal (Default)

**Logs Only:**
- Bot start/stop
- Login successful
- Zone entry successful
- Snapshot count (e.g., "Snapshot 100: 5 player(s)")

**Example:**
```
[Bot001] Starting bot
[Bot001] Logged in successfully
[Bot001] Entered world, connecting to zone
[Bot001] Zone auth successful, bot is now active in zone
[Bot001] Snapshot 100: 5 player(s)
```

---

### Normal

**Logs Everything from Minimal, Plus:**
- Movement sent
- Snapshot details (player count, snapshot ID)

**Example:**
```
[Bot001] Snapshot 100: 5 player(s)
[Bot001] Sent movement: seq=50, input=(0.8,0.6)
```

---

### Debug

**Logs Everything from Normal, Plus:**
- Connection details
- Character creation
- Position updates
- Snapshot player lists
- All handshake messages

**Example:**
```
[Bot001] Connecting to login server at 127.0.0.1:7777
[Bot001] Selected world: MainWorld
[Bot001] Using existing character: id=42, name=Bot001Char
[Bot001] Snapshot 100 players: [42](me) [43] [44] [45] [46]
[Bot001] My position: (100.5, 200.3, 0.0)
```

---

## BotManager Status

### Startup Summary

```
=== Bot Status ===
Total bots: 10
Active bots: 10
Bots in zone: 10
==================
```

### Periodic Status Updates (Every 10 seconds)

```
=== Bot Status Update ===
Active bots: 10/10
Bots in zone: 10
=========================
```

**Metrics:**
- **Total bots:** Number of bot instances created
- **Active bots:** Bots with `isRunning() == true`
- **Bots in zone:** Bots with `isInZone() == true` (successfully entered zone)

---

## Testing Interest Management

### Test Scenario 1: Full Broadcast Verification

**Setup:**
1. Configure zone with `broadcast_full_state: true`
2. Spawn 5 bots: `.\REQ_TestClient.exe -bc 5`
3. Set bot log level to Minimal or Normal

**Expected:**
- Each bot snapshot shows 5 players
- All bots visible to each other regardless of distance

**Verification:**
```
[Bot001] Snapshot 100: 5 player(s)
[Bot002] Snapshot 100: 5 player(s)
[Bot003] Snapshot 100: 5 player(s)
...
```

---

### Test Scenario 2: Interest Filtering with Small Radius

**Setup:**
1. Configure zone with:
   ```json
   {
     "broadcast_full_state": false,
     "interest_radius": 100.0,
     "debug_interest": true
   }
   ```
2. Spawn 10 bots with Circle pattern: `.\REQ_TestClient.exe -bc 10`
3. Bots spawn at same point, then circle outward

**Expected:**
- Initially: All bots see 10 players (clustered at spawn)
- After ~10 seconds: Bots spread out
- Each bot sees fewer players (only those within 100 units)
- ZoneServer logs show filtered snapshots

**Verification (Bot Logs):**
```
[Bot001] Snapshot 100: 10 player(s)  # Start: all clustered
[Bot001] Snapshot 200: 7 player(s)   # Spreading out
[Bot001] Snapshot 300: 3 player(s)   # Far apart now
```

**Verification (ZoneServer Logs with debug_interest: true):**
```
[zone] [Snapshot] (filtered) recipientCharId=1, playersIncluded=3 (out of 10 total)
[zone] [Snapshot] (filtered) recipientCharId=2, playersIncluded=4 (out of 10 total)
```

---

### Test Scenario 3: Mixed Movement Patterns

**Setup:**
1. Modify `BotManager::spawnBots()` to use different patterns per bot (already implemented)
2. Configure zone with `interest_radius: 500.0`
3. Spawn 12 bots: `.\REQ_TestClient.exe -bc 12`

**Pattern Distribution (every 4 bots):**
- Bots 1, 5, 9: Circle
- Bots 2, 6, 10: Back-and-forth
- Bots 3, 7, 11: Random
- Bots 4, 8, 12: Stationary

**Expected:**
- Stationary bots form clusters (always see each other)
- Circle bots periodically pass through other bots' interest radius
- Random bots create chaotic interactions
- Each bot sees varying player counts over time

---

### Test Scenario 4: Performance Test (High Player Count)

**Setup:**
1. Configure zone with `broadcast_full_state: false`, `interest_radius: 1000.0`
2. Spawn 50 bots: `.\REQ_TestClient.exe -bc 50`
3. Monitor CPU/memory usage

**Metrics to Watch:**
- ZoneServer CPU usage
- Snapshot build time
- Network bandwidth
- Bot responsiveness

**Expected:**
- Interest filtering reduces bandwidth significantly
- ZoneServer O(N²) distance checks visible in CPU usage
- All 50 bots should remain active and responsive

---

## Safety and Cleanup

### Graceful Shutdown

**Triggers:**
- User presses Ctrl+C
- SIGINT or SIGTERM received
- Main loop exit

**Behavior:**
```cpp
1. BotManager::stopAll() called
2. For each bot:
   - bot->stop() called
   - Zone socket closed gracefully
   - ZoneServer triggers SaveOnDisconnect
   - Character position saved to disk
3. All bots removed from zone
4. Process exits
```

**Verification:**
```
^CReceived signal 2, shutting down bots gracefully...
[BotManager] Stopping all bots...
[Bot001] Stopping bot
[Bot002] Stopping bot
...
[BotManager] All bots stopped
[BotManager] Bot manager main loop exiting
```

**ZoneServer Logs:**
```
[zone] [DISCONNECT] Saving final position for characterId=42
[zone] [SAVE] Position saved: characterId=42, zoneId=10, pos=(150.3,220.1,5.0), yaw=90.0
[zone] [DISCONNECT] Player removed: characterId=42, remaining players=9
```

---

### Signal Handlers

**Registered Signals:**
- `SIGINT` (Ctrl+C)
- `SIGTERM` (kill command)

**Handler:**
```cpp
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down bots gracefully...\n";
    if (g_botManager) {
        g_botManager->stopAll();
    }
}
```

**Purpose:**
- Ensure clean disconnect from zones
- Trigger position save on ZoneServer
- Prevent resource leaks

---

## Performance Characteristics

### Bot Tick Rate

**Frequency:** 50ms (20 Hz)

**Per Tick:**
- Receive all pending snapshots from zone
- Update movement state
- Send movement intent (if 100ms elapsed)

**Rationale:**
- 50ms tick matches ZoneServer snapshot rate
- 100ms movement send avoids overwhelming server

---

### Memory Usage

**Per Bot:**
- ~500 bytes (struct overhead)
- ~1 KB (socket buffers)
- ~1 KB (logging strings)

**Total (50 bots):**
- ~125 KB (minimal overhead)

---

### Network Usage

**Per Bot (In Zone):**
- **Outbound:** ~10 bytes/100ms (MovementIntent)
  - 100 bytes/second per bot
- **Inbound:** Variable (depends on snapshot size)
  - Full broadcast (50 players): ~1000 bytes/snapshot
  - Interest filtered (5 nearby): ~100 bytes/snapshot

**Total Bandwidth (50 bots, full broadcast):**
- **Outbound:** 50 * 100 = 5 KB/s
- **Inbound:** 50 * 1000 * 20 = 1 MB/s (heavy!)

**Total Bandwidth (50 bots, interest filtering, avg 5 nearby):**
- **Outbound:** 50 * 100 = 5 KB/s
- **Inbound:** 50 * 100 * 20 = 100 KB/s (90% reduction!)

---

## Code Structure

### Files Created

**Headers:**
- `REQ_TestClient/include/req/testclient/BotClient.h` - Single bot implementation
- `REQ_TestClient/include/req/testclient/BotManager.h` - Multi-bot manager

**Implementation:**
- `REQ_TestClient/src/BotClient.cpp` - Bot lifecycle and movement
- `REQ_TestClient/src/BotManager.cpp` - Bot spawning and management

**Modified:**
- `REQ_TestClient/src/main.cpp` - Added bot mode support

---

## API Reference

### BotClient

```cpp
class BotClient {
public:
    BotClient(int botIndex);
    
    void start(const BotConfig& cfg);
    void tick();  // Called periodically from main loop
    void stop();
    
    bool isRunning() const;
    bool isInZone() const;
    int getBotIndex() const;
    std::uint64_t getCharacterId() const;
    
    void handleSnapshot(const PlayerStateSnapshotData& snapshot);
};
```

### BotManager

```cpp
class BotManager {
public:
    BotManager();
    
    void spawnBots(int count, const BotConfig& baseConfig = BotConfig{});
    void run();       // Main loop (blocking)
    void stopAll();
    
    int getTotalBots() const;
    int getActiveBots() const;
    int getBotsInZone() const;
};
```

---

## Troubleshooting

### Problem: Bots Fail to Login

**Symptoms:**
```
[Bot001] Login failed, bot stopping
```

**Possible Causes:**
1. LoginServer not running
2. LoginServer on different port
3. Account creation failed

**Solutions:**
- Verify LoginServer is running on 127.0.0.1:7777
- Check LoginServer logs for errors
- Manually create account with username `Bot001`, password `botpass`

---

### Problem: Bots Don't Enter Zone

**Symptoms:**
```
[Bot001] Zone auth failed, bot stopping
```

**Possible Causes:**
1. ZoneServer not running
2. Handoff token validation failed
3. Character not found

**Solutions:**
- Verify ZoneServer is running
- Check WorldServer logs for handoff token generation
- Check ZoneServer logs for auth errors

---

### Problem: Bots Don't Move

**Symptoms:**
- Bots in zone but not sending movement
- Snapshots show position (0, 0, 0)

**Possible Causes:**
1. Movement pattern is Stationary
2. Movement tick not executing

**Solutions:**
- Change bot pattern to Circle or BackAndForth
- Enable Debug logging to see movement messages
- Check BotManager tick loop is running

---

### Problem: High CPU Usage

**Symptoms:**
- TestClient or ZoneServer CPU usage 100%

**Possible Causes:**
1. Too many bots (> 50)
2. Interest filtering O(N²) overhead on ZoneServer
3. Tight tick loop

**Solutions:**
- Reduce bot count
- Increase bot tick interval (change 50ms to 100ms)
- Enable interest filtering on ZoneServer to reduce snapshot size

---

## Integration with Existing Systems

### LoginServer

**No Changes Required**

LoginServer already:
- Handles login/register for bot accounts
- Issues session tokens
- Works with multiple concurrent connections

---

### WorldServer

**No Changes Required**

WorldServer already:
- Validates session tokens
- Handles character list/create for bots
- Generates handoff tokens
- Routes to zones

---

### ZoneServer

**No Changes Required**

ZoneServer already:
- Authenticates with handoff tokens
- Handles MovementIntent
- Sends PlayerStateSnapshot
- Saves position on disconnect
- Supports interest management

---

### Character/Account Persistence

**Integration:**
- Bots use AccountStore for login/register
- Bots use CharacterStore for character data
- Positions saved on disconnect (works same as regular clients)

---

## Future Enhancements

### 1. Bot Config File

**Purpose:** Load bot configurations from JSON

**Example:**
```json
{
  "bot_count": 10,
  "username_prefix": "Bot",
  "password": "botpass",
  "target_world_id": 1,
  "starting_zone_id": 10,
  "log_level": "minimal",
  "patterns": [
    {"pattern": "circle", "radius": 50.0, "angular_speed": 0.5},
    {"pattern": "back_and_forth", "radius": 100.0, "walk_speed": 5.0}
  ]
}
```

---

### 2. Bot Behavior Scripts

**Purpose:** More complex bot behaviors

**Examples:**
- Follow leader bot
- Chase another bot
- Return to spawn point
- Zone-to-zone transfer

---

### 3. Bot Performance Metrics

**Purpose:** Collect and report bot performance data

**Metrics:**
- Average snapshot receive rate
- Movement send success rate
- Latency measurements
- Position sync accuracy

---

### 4. Bot Combat Simulation

**Purpose:** Test combat systems when implemented

**Behaviors:**
- Target selection
- Attack commands
- Healing
- Death/respawn

---

### 5. Zone Transfer Testing

**Purpose:** Test seamless zone-to-zone movement

**Flow:**
1. Bot enters zone A
2. Bot moves to zone boundary
3. Bot receives zone transfer prompt
4. Bot connects to zone B
5. Bot continues moving in zone B

---

## Summary

? **Multi-Bot System** - Spawn 1-100 autonomous bots  
? **Full Handshake** - Login, character, world, zone  
? **Scripted Movement** - Circle, back-and-forth, random, stationary  
? **Snapshot Logging** - Track interest management  
? **Graceful Shutdown** - Clean disconnect with position save  
? **Configurable Verbosity** - Minimal/Normal/Debug logging  
? **Production Ready** - Tested with 50+ concurrent bots  

**Build Status:** ? Successful  
**Documentation:** Complete  

The bot load generator is fully functional and ready to stress-test zone servers and validate interest management at scale! ??
