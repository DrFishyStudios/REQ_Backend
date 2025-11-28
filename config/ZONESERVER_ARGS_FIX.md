# ZoneServer Command-Line Argument Parsing - Fix Documentation

## Problem Summary
ZoneServer processes were being auto-launched by WorldServer with unique zone IDs, but all instances were reporting `zoneId=1` because the command-line arguments were being ignored and hard-coded values were used instead.

## Root Cause
The original implementation had:
1. Hard-coded `zoneId = 1` in `main.cpp`
2. No command-line argument parsing
3. Constructor only accepted 3 parameters (address, port, zoneId) without worldId

## Solution Implemented

### 1. Updated ZoneServer Constructor
**File:** `REQ_ZoneServer/include/req/zone/ZoneServer.h`

**Before:**
```cpp
ZoneServer(const std::string& address,
           std::uint16_t port,
           req::shared::ZoneId zoneId);
```

**After:**
```cpp
ZoneServer(std::uint32_t worldId,
           std::uint32_t zoneId,
           const std::string& address,
           std::uint16_t port);
```

**Changes:**
- Added `worldId` parameter (first parameter)
- Reordered parameters for logical grouping (IDs first, then network config)
- Added member variables: `worldId_`, `address_`, `port_` (in addition to existing `zoneId_`)

### 2. Enhanced Constructor Implementation
**File:** `REQ_ZoneServer/src/ZoneServer.cpp`

**Key Changes:**
- Stores all constructor parameters in member variables
- Logs detailed construction info:
  ```
  [INFO] ZoneServer constructed:
  [INFO]   worldId=1
  [INFO]   zoneId=10
  [INFO]   address=0.0.0.0
  [INFO]   port=7000
  ```

### 3. Improved Startup Logging
**File:** `REQ_ZoneServer/src/ZoneServer.cpp` - `run()` method

**Enhanced Log Output:**
```
[INFO] ZoneServer starting: worldId=1, zoneId=10, address=0.0.0.0, port=7000
```

This single line shows all critical configuration at startup, making it easy to verify each zone has correct settings.

### 4. Comprehensive Command-Line Parsing
**File:** `REQ_ZoneServer/src/main.cpp`

**Supported Arguments:**
- `--world_id=<value>` - World server ID this zone belongs to
- `--zone_id=<value>` - Unique zone identifier
- `--port=<value>` - TCP port to listen on (1-65535)
- `--address=<value>` - IP address to bind to (optional, defaults to 0.0.0.0)

**Default Values:**
```cpp
std::uint32_t worldId = 1;      // Default if --world_id not provided
std::uint32_t zoneId = 1;       // Default if --zone_id not provided
std::string address = "0.0.0.0"; // Default if --address not provided
std::uint16_t port = 7779;      // Default if --port not provided
```

**Parsing Logic:**
1. Loops through all command-line arguments (`argv[1]` to `argv[argc-1]`)
2. For each argument matching `--key=value` format:
   - Extracts the value after `=`
   - Converts to appropriate type with error checking
   - Logs the parsed value
3. Tracks which values were provided vs defaulted
4. Logs warnings for any defaults used
5. Logs final configuration summary

**Error Handling:**
- Invalid number format ? logs error and exits with code 1
- Port out of range (0 or >65535) ? logs error and exits
- Unknown arguments ? logs warning but continues
- Parse exceptions ? caught and logged with details

## Logging Output Examples

### Successful Auto-Launch (Zone 10)
```
[INFO] [Main] Parsing 3 command-line argument(s)
[INFO] [Main]   Parsed --world_id=1
[INFO] [Main]   Parsed --zone_id=10
[INFO] [Main]   Parsed --port=7000
[INFO] [Main] Final ZoneServer configuration:
[INFO] [Main]   worldId=1
[INFO] [Main]   zoneId=10
[INFO] [Main]   address=0.0.0.0
[INFO] [Main]   port=7000
[INFO] [zone] ZoneServer constructed:
[INFO] [zone]   worldId=1
[INFO] [zone]   zoneId=10
[INFO] [zone]   address=0.0.0.0
[INFO] [zone]   port=7000
[INFO] [zone] ZoneServer starting: worldId=1, zoneId=10, address=0.0.0.0, port=7000
[INFO] [zone] Entering IO event loop...
```

### Manual Start (Using Defaults)
```
[INFO] [Main] Parsing 0 command-line argument(s)
[WARN] [Main] Using DEFAULT worldId=1 (--world_id not provided)
[WARN] [Main] Using DEFAULT zoneId=1 (--zone_id not provided)
[WARN] [Main] Using DEFAULT port=7779 (--port not provided)
[INFO] [Main] Final ZoneServer configuration:
[INFO] [Main]   worldId=1
[INFO] [Main]   zoneId=1
[INFO] [Main]   address=0.0.0.0
[INFO] [Main]   port=7779
```

### Error Cases

#### Invalid Port Value
```
[INFO] [Main]   Parsed --world_id=1
[INFO] [Main]   Parsed --zone_id=10
[ERROR] [Main] Invalid port value: 99999 (must be 1-65535)
```

#### Parse Error
```
[ERROR] [Main] Failed to parse --zone_id value 'abc': invalid stoul argument
```

#### Unknown Argument
```
[WARN] [Main] Unknown command-line argument: --invalid_option=123
```

## WorldServer Auto-Launch Integration

When WorldServer auto-launches zones with `world_config.json`, it now passes:

### Example Zone Configuration
```json
{
  "zone_id": 10,
  "zone_name": "East Freeport",
  "host": "127.0.0.1",
  "port": 7000,
  "executable_path": "./REQ_ZoneServer.exe",
  "args": [
    "--world_id=1",
    "--zone_id=10",
    "--port=7000"
  ]
}
```

### WorldServer Spawn Command
```
./REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7000
```

### Result
Each zone now correctly identifies itself:
- Zone 10 (East Freeport) ? `worldId=1, zoneId=10, port=7000`
- Zone 20 (North Ro) ? `worldId=1, zoneId=20, port=7001`
- Zone 30 (Nektulos Forest) ? `worldId=1, zoneId=30, port=7002`

## Verification Steps

### 1. Check WorldServer Logs
Look for spawn commands:
```
[INFO] [world] Spawning process: ./REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7000
[INFO] [world] Process created successfully - PID: 12345
```

### 2. Check Each ZoneServer Console
Each zone should show unique configuration:
```
[INFO] [zone] ZoneServer starting: worldId=1, zoneId=10, address=0.0.0.0, port=7000
```

### 3. Verify Client Connections
When TestClient connects to a zone, the welcome message should show correct zone:
```
[INFO] [TestClient] Zone entry successful: Welcome to zone 10 on world 1 (character 42)
```

### 4. Check Task Manager (Windows)
Multiple `REQ_ZoneServer.exe` processes should be running, each listening on different ports.

## Manual Testing

### Test 1: With Full Arguments
```bash
cd x64/Debug
.\REQ_ZoneServer.exe --world_id=1 --zone_id=99 --port=8888
```

**Expected Output:**
```
[INFO] [Main]   Parsed --world_id=1
[INFO] [Main]   Parsed --zone_id=99
[INFO] [Main]   Parsed --port=8888
[INFO] [zone] ZoneServer starting: worldId=1, zoneId=99, address=0.0.0.0, port=8888
```

### Test 2: With Defaults
```bash
.\REQ_ZoneServer.exe
```

**Expected Output:**
```
[WARN] [Main] Using DEFAULT worldId=1 (--world_id not provided)
[WARN] [Main] Using DEFAULT zoneId=1 (--zone_id not provided)
[WARN] [Main] Using DEFAULT port=7779 (--port not provided)
```

### Test 3: Invalid Arguments
```bash
.\REQ_ZoneServer.exe --zone_id=abc
```

**Expected Output:**
```
[ERROR] [Main] Failed to parse --zone_id value 'abc': ...
```
Exit code: 1

## Code Changes Summary

### Files Modified
1. **REQ_ZoneServer/include/req/zone/ZoneServer.h**
   - Updated constructor signature
   - Added `worldId_`, `address_`, `port_` member variables

2. **REQ_ZoneServer/src/ZoneServer.cpp**
   - Updated constructor implementation
   - Enhanced logging in constructor and `run()`
   - Updated welcome message to include worldId

3. **REQ_ZoneServer/src/main.cpp**
   - Complete rewrite of argument parsing
   - Added validation and error handling
   - Comprehensive logging of configuration

### Lines Changed
- **ZoneServer.h**: ~10 lines (constructor signature, member variables)
- **ZoneServer.cpp**: ~30 lines (constructor, logging updates)
- **main.cpp**: ~100 lines (complete argument parsing system)

## Future Enhancements

### TODO Items
1. **Configuration File Support**
   - Load zone config from JSON file
   - Combine command-line args with file config
   - Command-line args override file values

2. **Additional Arguments**
   - `--log_level=DEBUG|INFO|WARN|ERROR`
   - `--zone_name="Custom Name"`
   - `--max_players=100`
   - `--enable_pvp=true/false`

3. **Validation**
   - Verify zone_id exists in world's zone list
   - Check port is not already in use
   - Validate world_id with world server registry

4. **Help System**
   - `--help` flag to show usage
   - List all available arguments
   - Show default values

5. **Environment Variables**
   - Support `REQ_WORLD_ID`, `REQ_ZONE_ID` env vars
   - Precedence: command-line > env vars > config file > defaults

## Best Practices

### When Adding New Arguments
1. Add to default values at top of `main()`
2. Add parsing case in argument loop
3. Add validation if needed
4. Log the parsed value
5. Include in final configuration summary
6. Update documentation

### Logging Guidelines
- **Parse success**: INFO level with parsed value
- **Using default**: WARN level (makes it obvious in logs)
- **Parse error**: ERROR level with details
- **Unknown argument**: WARN level (non-fatal)
- **Final config**: INFO level summary

### Error Handling
- Always catch parse exceptions
- Log detailed error messages
- Return non-zero exit code on fatal errors
- Continue on warnings (unknown args, defaults used)

## Troubleshooting

### Problem: All zones still show zoneId=1

**Check:**
1. WorldServer spawn command in logs - are arguments being passed?
2. ZoneServer argument parsing logs - are arguments being received?
3. Make sure you rebuilt REQ_ZoneServer after code changes

### Problem: Zone fails to start with parse error

**Check:**
1. Argument format: must be `--key=value` (no spaces around `=`)
2. Value type: zone_id and port must be integers
3. Port range: must be 1-65535

### Problem: WorldServer spawns but zones don't connect

**Check:**
1. Each zone has unique port number
2. Ports are not blocked by firewall
3. Zone servers show "Entering IO event loop..." in logs
4. WorldServer is sending correct host/port to clients

## Integration with Other Systems

### LoginServer
- No changes required
- Already returns world list with correct worldId

### WorldServer
- Auto-launch passes `--world_id` matching world config
- Handoff tokens include zone information
- TODO: Validate zone belongs to this world

### TestClient
- Receives zone endpoint from WorldServer
- Connects to correct zone based on port
- Welcome message shows correct zone ID

## Performance Considerations

### Argument Parsing
- O(n) where n = number of arguments (typically <10)
- String operations are minimal
- No heap allocations in hot path
- Parsing happens once at startup

### Logging
- All startup logging is one-time cost
- No impact on runtime performance
- Can be filtered by log level if needed

## Security Considerations

### Input Validation
- Port range validated (1-65535)
- Integer parsing uses exception handling
- No buffer overflows (using std::string)

### TODO: Add Additional Validation
- Whitelist allowed zone IDs
- Validate world ID against registry
- Sanitize address string (prevent injection)
- Maximum length checks for all strings
