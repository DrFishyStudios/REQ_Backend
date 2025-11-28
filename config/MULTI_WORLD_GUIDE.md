# Multi-World Support Implementation Guide

## Overview
The REQ_Backend now supports running multiple independent world servers, each with its own configuration, while maintaining a centralized world list for the login server. This enables horizontal scaling and server variety (different rulesets, regions, etc.).

## Architecture

### Before
- WorldServer loaded hardcoded `config/world_config.json`
- LoginServer returned hardcoded or single-world list
- No way to run multiple worlds simultaneously

### After
- WorldServer accepts `--config=<path>` to load any world config
- LoginServer loads `config/worlds.json` with all available worlds
- Multiple WorldServer processes can run on same or different machines
- Centralized world list management

## Changes Summary

### REQ_WorldServer

**File:** `src/main.cpp`

**Changes:**
- ? Added command-line argument parsing for `--config=<path>`
- ? Default: `config/world_config.json`
- ? Enhanced logging shows which config file is loaded
- ? All world configuration comes from loaded config (no hardcoding)

**Usage:**
```bash
# Default config
.\REQ_WorldServer.exe

# Custom config
.\REQ_WorldServer.exe --config=config/world_cazic_thule.json
.\REQ_WorldServer.exe --config=config/world_karana.json
```

### REQ_Shared

**File:** `include/req/shared/Config.h`

**New Structures:**
```cpp
struct LoginWorldEntry {
    std::uint32_t worldId;
    std::string worldName;
    std::string host;
    std::uint16_t port;
    std::string rulesetId;
};

struct WorldListConfig {
    std::vector<LoginWorldEntry> worlds;
};
```

**New Function:**
```cpp
WorldListConfig loadWorldListConfig(const std::string& path);
```

**File:** `src/Config.cpp`

**Implementation:**
- ? Parses `worlds.json` with array of world entries
- ? Validates all required fields
- ? Validates port ranges
- ? Comprehensive error logging
- ? Detailed success logging for each world

### REQ_LoginServer

**File:** `include/req/login/LoginServer.h`

**Changes:**
- ? Constructor now accepts `WorldListConfig` parameter
- ? Stores `std::vector<LoginWorldEntry> worlds_`

**File:** `src/LoginServer.cpp`

**Changes:**
- ? Loads world list in constructor
- ? Builds `LoginResponse` from loaded world list
- ? Logs each world sent to client
- ? Converts `LoginWorldEntry` to `ProtocolSchemas::WorldListEntry`

**File:** `src/main.cpp`

**Changes:**
- ? Loads `config/worlds.json` on startup
- ? Passes `WorldListConfig` to `LoginServer` constructor
- ? Logs number of worlds loaded

## Configuration Files

### worlds.json (LoginServer)
**Location:** `config/worlds.json`

**Purpose:** Central list of all available worlds for server selection

**Schema:**
```json
{
  "worlds": [
    {
      "world_id": 1,
      "world_name": "CazicThule",
      "host": "127.0.0.1",
      "port": 7778,
      "ruleset_id": "classic_plus_qol"
    },
    {
      "world_id": 2,
      "world_name": "Karana",
      "host": "127.0.0.1",
      "port": 7780,
      "ruleset_id": "classic_hardcore"
    }
  ]
}
```

**Fields:**
- `world_id` (required): Unique world identifier
- `world_name` (required): Display name shown to players
- `host` (required): Hostname/IP where world server listens
- `port` (required): Port where world server listens (1-65535)
- `ruleset_id` (required): Ruleset identifier for this world

**Validation:**
- At least one world must be defined
- `world_name` cannot be empty
- `port` must be in valid range
- Missing required fields cause startup failure

### world_<name>.json (WorldServer)
**Location:** `config/world_cazic_thule.json`, `config/world_karana.json`, etc.

**Purpose:** Individual world server configuration

**Schema:** Same as before (see `world_config.json`)

**Key Point:** The `world_id`, `world_name`, `address`, `port` in each world config should match the corresponding entry in `worlds.json`.

## Logging Examples

### WorldServer Startup with --config

**CazicThule World:**
```
[INFO] [Main] Command-line: using config file: config/world_cazic_thule.json
[INFO] [Main] Loading world configuration from: config/world_cazic_thule.json
[INFO] [Config] Loading WorldConfig from: config/world_cazic_thule.json
[INFO] [Config] WorldConfig loaded: worldId=1, worldName=CazicThule, address=0.0.0.0, port=7778, rulesetId=classic_plus_qol, zones=3, autoLaunchZones=true
[INFO] [Main] Configuration loaded successfully:
[INFO] [Main]   worldId=1
[INFO] [Main]   worldName=CazicThule
[INFO] [Main]   address=0.0.0.0:7778
[INFO] [Main]   rulesetId=classic_plus_qol
[INFO] [Main]   zones=3
[INFO] [Main]   autoLaunchZones=true
[INFO] [world] WorldServer starting: worldId=1, worldName=CazicThule
[INFO] [world] Listening on 0.0.0.0:7778
```

**Karana World:**
```
[INFO] [Main] Command-line: using config file: config/world_karana.json
[INFO] [Main] Loading world configuration from: config/world_karana.json
[INFO] [Config] WorldConfig loaded: worldId=2, worldName=Karana, address=0.0.0.0, port=7780, rulesetId=classic_hardcore, zones=2, autoLaunchZones=true
[INFO] [Main]   worldId=2
[INFO] [Main]   worldName=Karana
[INFO] [Main]   address=0.0.0.0:7780
[INFO] [Main]   rulesetId=classic_hardcore
```

### LoginServer Startup

```
[INFO] [Main] Loading configuration...
[INFO] [Config] Loading LoginConfig from: config/login_config.json
[INFO] [Main] LoginConfig loaded: address=0.0.0.0, port=7777
[INFO] [Main] Loading world list...
[INFO] [Config] Loading WorldListConfig from: config/worlds.json
[INFO] [Config] WorldListConfig loaded: 2 world(s)
[INFO] [Config]   World: id=1, name=CazicThule, endpoint=127.0.0.1:7778, ruleset=classic_plus_qol
[INFO] [Config]   World: id=2, name=Karana, endpoint=127.0.0.1:7780, ruleset=classic_hardcore
[INFO] [Main] WorldList loaded: 2 world(s) available
[INFO] [login] LoginServer initialized with 2 world(s)
```

### LoginResponse with Multiple Worlds

```
[INFO] [login] LoginRequest: username=testuser, clientVersion=0.1.0
[INFO] [login] LoginResponse OK: username=testuser, sessionToken=123456789, worldCount=2
[INFO] [login]   World: id=1, name=CazicThule, endpoint=127.0.0.1:7778, ruleset=classic_plus_qol
[INFO] [login]   World: id=2, name=Karana, endpoint=127.0.0.1:7780, ruleset=classic_hardcore
```

## Running Multiple Worlds

### Start Script Example (Windows)

**start_all_servers.bat:**
```batch
@echo off
echo Starting REQ Backend Servers...

echo.
echo Starting LoginServer...
start "REQ_LoginServer" /D "x64\Debug" REQ_LoginServer.exe

timeout /t 2

echo.
echo Starting CazicThule WorldServer...
start "WorldServer_CazicThule" /D "x64\Debug" REQ_WorldServer.exe --config=config/world_cazic_thule.json

timeout /t 2

echo.
echo Starting Karana WorldServer...
start "WorldServer_Karana" /D "x64\Debug" REQ_WorldServer.exe --config=config/world_karana.json

echo.
echo All servers started!
pause
```

### Manual Start

**Terminal 1 - LoginServer:**
```bash
cd x64/Debug
.\REQ_LoginServer.exe
```

**Terminal 2 - CazicThule World:**
```bash
cd x64/Debug
.\REQ_WorldServer.exe --config=config/world_cazic_thule.json
```

**Terminal 3 - Karana World:**
```bash
cd x64/Debug
.\REQ_WorldServer.exe --config=config/world_karana.json
```

## TestClient Behavior

### Updated Flow

1. **Connect to LoginServer**
2. **Receive world list** (multiple worlds)
3. **Log all worlds:**
   ```
   [INFO] [TestClient] Selected world: CazicThule (ruleset: classic_plus_qol)
   ```
4. **Select first world** (current behavior)
5. **Continue handshake** to selected world

### Future Enhancement
TestClient could:
- Display world selection menu
- Allow user to choose world
- Store last selected world

## Port Assignment Strategy

### LoginServer
- Port: `7777` (fixed in `login_config.json`)

### WorldServers
- CazicThule: `7778` (in `world_cazic_thule.json`)
- Karana: `7780` (in `world_karana.json`)
- Future worlds: `7782`, `7784`, etc.

### ZoneServers (per world)
- CazicThule zones: `7000-7099`
- Karana zones: `7100-7199`
- Future worlds: Continue pattern

**Key Point:** Each world's zones should use non-overlapping port ranges.

## Validation Checklist

### Configuration Consistency

#### worlds.json ? world_*.json
For each world in `worlds.json`:
- [ ] `world_id` matches world config
- [ ] `world_name` matches world config
- [ ] `host` is accessible from clients
- [ ] `port` matches world config listen port
- [ ] `ruleset_id` matches world config

### Startup Verification

#### LoginServer
- [ ] Loads `login_config.json` successfully
- [ ] Loads `worlds.json` successfully
- [ ] Logs correct number of worlds
- [ ] Each world logged with all fields
- [ ] Listens on expected port (7777)

#### WorldServer (for each instance)
- [ ] Accepts `--config` argument
- [ ] Loads correct world config file
- [ ] Logs unique worldId and worldName
- [ ] Listens on unique port
- [ ] No port conflicts with other worlds
- [ ] Zone ports don't conflict

### Client Connection

#### TestClient
- [ ] Connects to LoginServer
- [ ] Receives multiple worlds in LoginResponse
- [ ] Logs all received worlds
- [ ] Successfully connects to selected world
- [ ] Handshake completes to zone

## Troubleshooting

### Problem: WorldServer ignores --config flag

**Check:**
1. Command-line syntax: `--config=<path>` (no spaces around `=`)
2. Path is relative to working directory
3. Config file exists at specified path
4. Logs show "Command-line: using config file:"

### Problem: LoginServer shows 0 worlds

**Check:**
1. `config/worlds.json` exists
2. JSON syntax is valid
3. `worlds` array is not empty
4. All required fields present
5. Check LoginServer logs for parse errors

### Problem: Port conflicts

**Symptoms:**
```
[ERROR] acceptor bind failed: Address already in use
```

**Solutions:**
1. Ensure each world uses unique port
2. Check no other processes using ports
3. Kill existing server processes
4. Verify `worlds.json` and world configs have matching ports

### Problem: Client can't connect to world

**Check:**
1. WorldServer is running (check task manager)
2. WorldServer logs show it's listening
3. Port in `worlds.json` matches world config
4. Firewall not blocking port
5. `host` in `worlds.json` is correct for client's network

## Production Deployment

### Single Server Deployment
```
Server Machine:
  - LoginServer: 0.0.0.0:7777
  - World 1: 0.0.0.0:7778 (zones: 7000-7099)
  - World 2: 0.0.0.0:7780 (zones: 7100-7199)
  
worlds.json:
  - World 1: <public_ip>:7778
  - World 2: <public_ip>:7780
```

### Multi-Server Deployment
```
Login Server (server1.example.com):
  - LoginServer: 0.0.0.0:7777
  
World Server 1 (server2.example.com):
  - WorldServer: 0.0.0.0:7778
  
World Server 2 (server3.example.com):
  - WorldServer: 0.0.0.0:7778  (same port, different machine)
  
worlds.json:
  - World 1: server2.example.com:7778
  - World 2: server3.example.com:7778
```

### Load Balancing Considerations
- Each world is independent
- Load balance by player distribution across worlds
- No cross-world communication needed (for now)
- Future: Zone servers could be on separate machines per world

## Security Considerations

### Configuration Files
- Store configs outside web-accessible directories
- Restrict read permissions to server process user
- Don't commit sensitive data to version control
- Use environment variables for production hosts/ports

### Network Topology
- LoginServer should be publicly accessible
- WorldServers need public access (or VPN)
- ZoneServers can be internal (NAT/port forwarding)
- Consider firewall rules per server type

## Future Enhancements

### Dynamic World List
- [ ] Database-backed world list
- [ ] World status (online/offline/full)
- [ ] Player population per world
- [ ] World capacity limits

### World Server Registry
- [ ] WorldServer auto-registers with LoginServer
- [ ] Heartbeat/health checks
- [ ] Automatic removal of offline worlds
- [ ] Dynamic world addition without LoginServer restart

### Advanced Features
- [ ] Cross-world chat
- [ ] Character transfers between worlds
- [ ] Instanced zones across worlds
- [ ] Geographic region affinity

## Migration Guide

### From Single World

**Before:**
```cpp
// LoginServer hardcoded world
world.worldId = 1;
world.worldName = "MainWorld";
world.worldHost = "127.0.0.1";
world.worldPort = 7778;
```

**After:**
1. Create `config/worlds.json` with your world(s)
2. Rename `world_config.json` to `world_<name>.json`
3. Update LoginServer to load worlds.json
4. Start WorldServer with `--config=<path>`

### Adding New Worlds

1. **Create world config:** `config/world_newworld.json`
   - Unique `world_id`
   - Unique `port`
   - Unique zone port ranges

2. **Update worlds.json:**
   - Add new entry with matching id/name/port
   - Verify JSON syntax

3. **Restart LoginServer** to load new world list

4. **Start new WorldServer:**
   ```bash
   .\REQ_WorldServer.exe --config=config/world_newworld.json
   ```

## Summary

**Status:** ? Fully Implemented

**Key Benefits:**
- Run multiple worlds on same or different servers
- Each world can have different rulesets
- Centralized world list management
- Clear logging for debugging multi-world setups
- Flexible deployment options

**Testing:**
- Build: Successful
- Configuration files created
- Ready for multi-world testing

**Next Steps:**
1. Start all servers (Login + 2 Worlds)
2. Run TestClient to verify world list
3. Test connection to each world
4. Verify zone servers spawn correctly per world
