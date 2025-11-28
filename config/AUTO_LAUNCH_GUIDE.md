# Zone Server Auto-Launch Feature

## Overview
The WorldServer can now automatically spawn Zone Server processes on startup based on configuration. This eliminates the need to manually start each zone server when `auto_launch_zones` is enabled in `world_config.json`.

## Configuration

### world_config.json
```json
{
  "world_id": 1,
  "world_name": "CazicThule",
  "address": "0.0.0.0",
  "port": 7778,
  "ruleset_id": "classic_plus_qol",
  "auto_launch_zones": true,    // Enable auto-launch
  "zones": [
    {
      "zone_id": 10,
      "zone_name": "East Freeport",
      "host": "127.0.0.1",
      "port": 7000,
      "executable_path": "./REQ_ZoneServer.exe",  // Required for auto-launch
      "args": [                                     // Command-line arguments
        "--world_id=1",
        "--zone_id=10",
        "--port=7000"
      ]
    }
  ]
}
```

### Required Fields for Auto-Launch
Each zone entry must have:
- `executable_path`: Path to the zone server executable (relative or absolute)
- `args`: Array of command-line arguments to pass to the zone server
- `port`: Valid port number (1-65535)

## How It Works

### Startup Sequence
1. **WorldServer Constructor**
   - Loads `world_config.json`
   - Logs world configuration details
   - Logs each configured zone with executable path

2. **WorldServer::run()**
   - Before entering the IO event loop
   - Checks `config.autoLaunchZones`
   - If `true`, calls `launchConfiguredZones()`

3. **launchConfiguredZones()**
   - Iterates through all zones in `config.zones`
   - Validates each zone (executable path, port)
   - Calls `spawnZoneProcess()` for each valid zone
   - Logs success/failure for each spawn attempt
   - Logs summary (X succeeded, Y failed)

4. **spawnZoneProcess()** (Windows)
   - Builds command line with proper quoting
   - Uses Windows `CreateProcessA` API
   - Creates zone server in new console window
   - Logs process ID on success
   - Logs error code on failure
   - Returns immediately (non-blocking)

## Logging Output

### Successful Auto-Launch
```
[INFO] [Config] Loading WorldConfig from: config/world_config.json
[INFO] [Config] WorldConfig loaded: worldId=1, worldName=CazicThule, address=0.0.0.0, port=7778, rulesetId=classic_plus_qol, zones=3, autoLaunchZones=true
[INFO] [Config]   Zone: id=10, name=East Freeport, endpoint=127.0.0.1:7000, executable=./REQ_ZoneServer.exe
[INFO] [Config]   Zone: id=20, name=North Ro, endpoint=127.0.0.1:7001, executable=./REQ_ZoneServer.exe
[INFO] [Config]   Zone: id=30, name=Nektulos Forest, endpoint=127.0.0.1:7002, executable=./REQ_ZoneServer.exe
[INFO] [world] WorldServer constructed:
[INFO] [world]   worldId=1
[INFO] [world]   worldName=CazicThule
[INFO] [world]   autoLaunchZones=true
[INFO] [world]   zones.size()=3
[INFO] [world] WorldServer starting: worldId=1, worldName=CazicThule
[INFO] [world] Listening on 0.0.0.0:7778
[INFO] [world] Ruleset: classic_plus_qol, zones=3, autoLaunchZones=true
[INFO] [world] Auto-launch is ENABLED - attempting to spawn zone processes
[INFO] [world] launchConfiguredZones: processing 3 zone(s)
[INFO] [world] Processing zone: id=10, name=East Freeport
[INFO] [world]   endpoint=127.0.0.1:7000
[INFO] [world]   executable=./REQ_ZoneServer.exe
[INFO] [world]   args.size()=3
[INFO] [world] Spawning process: ./REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7000
[INFO] [world] Process created successfully - PID: 12345
[INFO] [world] ? Successfully launched zone 10 (East Freeport)
[INFO] [world] Processing zone: id=20, name=North Ro
[INFO] [world]   endpoint=127.0.0.1:7001
[INFO] [world]   executable=./REQ_ZoneServer.exe
[INFO] [world]   args.size()=3
[INFO] [world] Spawning process: ./REQ_ZoneServer.exe --world_id=1 --zone_id=20 --port=7001
[INFO] [world] Process created successfully - PID: 12346
[INFO] [world] ? Successfully launched zone 20 (North Ro)
[INFO] [world] Processing zone: id=30, name=Nektulos Forest
[INFO] [world]   endpoint=127.0.0.1:7002
[INFO] [world]   executable=./REQ_ZoneServer.exe
[INFO] [world]   args.size()=3
[INFO] [world] Spawning process: ./REQ_ZoneServer.exe --world_id=1 --zone_id=30 --port=7002
[INFO] [world] Process created successfully - PID: 12347
[INFO] [world] ? Successfully launched zone 30 (Nektulos Forest)
[INFO] [world] Auto-launch summary: 3 succeeded, 0 failed
[INFO] [world] Entering IO event loop...
```

### Error Cases

#### Missing Executable Path
```
[ERROR] [world] Zone 10 (East Freeport) has empty executable_path - skipping
```

#### Invalid Port
```
[ERROR] [world] Zone 20 (North Ro) has invalid port 0 - skipping
```

#### CreateProcess Failure
```
[INFO] [world] Spawning process: ./REQ_ZoneServer.exe --zone_id=10
[ERROR] [world] CreateProcess failed with error code: 2
[ERROR] [world] ? Failed to launch zone 10 (East Freeport)
```
Error code 2 = File not found

#### Auto-Launch Disabled
```
[INFO] [world] Auto-launch is DISABLED - zone processes expected to be managed externally
```

## Platform Support

### Windows (Implemented)
- Uses `CreateProcessA` API
- Creates each zone in a new console window (`CREATE_NEW_CONSOLE`)
- Non-blocking (doesn't wait for child processes)
- Handles spaces in paths and arguments with proper quoting
- Logs process ID (PID) on success
- Logs error codes on failure

### Linux/macOS (Not Implemented)
```
[WARN] [world] Auto-launch not implemented on this platform (non-Windows)
[WARN] [world] Zone 10 must be started manually
```
**TODO:** Implement using `fork()` + `exec()` or `posix_spawn()`

## Testing

### Manual Testing
1. Configure `world_config.json` with `auto_launch_zones: true`
2. Ensure `executable_path` points to a valid zone server executable
3. Start WorldServer
4. Check logs for auto-launch messages
5. Verify zone server processes appear in Task Manager (Windows) or `ps` (Unix)

### Verification Checklist
- [ ] WorldConfig loads without errors
- [ ] Config logging shows all zones with executable paths
- [ ] Auto-launch enabled message appears
- [ ] Each zone shows "Processing zone" message
- [ ] Each zone shows spawn attempt with full command line
- [ ] Success messages show PIDs
- [ ] Auto-launch summary shows correct counts
- [ ] Zone server processes are running
- [ ] Zone servers can accept connections

## Troubleshooting

### Zone Servers Not Starting

1. **Check executable_path**
   - Verify path is correct (relative to WorldServer working directory)
   - Check file exists and is executable
   - Try absolute path if relative path fails

2. **Check arguments**
   - Ensure arguments are valid for ZoneServer
   - Check for typos in argument names
   - Verify port numbers don't conflict

3. **Check logs**
   - Look for error codes in WorldServer logs
   - Check zone server console windows (if they appear briefly then close)
   - Windows error codes:
     - 2 = File not found
     - 5 = Access denied
     - 267 = Directory name invalid

4. **Check permissions**
   - Ensure WorldServer has permission to spawn processes
   - Ensure zone server executable is not blocked by antivirus

### Auto-Launch Not Happening

1. **Verify auto_launch_zones setting**
   ```
   [INFO] [world] Auto-launch is DISABLED
   ```
   If you see this, check `world_config.json` has `"auto_launch_zones": true`

2. **Check config loading**
   - Look for config parse errors
   - Verify JSON syntax is valid
   - Check zones array is not empty

## Future Enhancements

### TODO Items
1. **Process Management**
   - Track spawned process handles
   - Monitor zone server health
   - Restart failed zones automatically
   - Graceful shutdown of child processes

2. **Cross-Platform Support**
   - Implement Linux/macOS process spawning
   - Use `fork()` + `exec()` or `posix_spawn()`
   - Handle platform-specific argument passing

3. **Configuration**
   - Support for environment variables in config
   - Working directory per zone
   - Resource limits (CPU, memory)
   - Logging redirection for zone servers

4. **Integration**
   - Validate handoff tokens between world and zones
   - Health checks to verify zones are responsive
   - Automatic zone selection based on load
   - Dynamic zone spawning based on player count

## Implementation Details

### File Locations
- **WorldServer.h**: Declaration of `launchConfiguredZones()` and `spawnZoneProcess()`
- **WorldServer.cpp**: Implementation with Windows-specific `CreateProcessA` code
- **Config.cpp**: Enhanced logging to show zone details including executable paths

### Code Flow
```
WorldServer::run()
  ??> Check config_.autoLaunchZones
  ??> launchConfiguredZones()
       ??> For each zone in config_.zones
       ?    ??> Validate executable_path and port
       ?    ??> spawnZoneProcess(zone)
       ?         ??> Build command line with quoting
       ?         ??> CreateProcessA (Windows)
       ?         ??> Log PID or error code
       ??> Log summary (X succeeded, Y failed)
```

### Windows API Details
- **CreateProcessA**: Creates new process
  - `CREATE_NEW_CONSOLE`: Each zone gets own console window
  - Non-inherited handles: Zone servers independent of WorldServer
  - Immediate handle closure: No waiting for child processes
  
### Security Considerations
- Command-line injection: Arguments are quoted but not sanitized
- **TODO:** Add validation of executable paths
- **TODO:** Whitelist allowed executables
- **TODO:** Validate argument format to prevent injection

## Example Configurations

### Single Zone (Development)
```json
{
  "auto_launch_zones": true,
  "zones": [{
    "zone_id": 1,
    "zone_name": "Test Zone",
    "host": "127.0.0.1",
    "port": 7779,
    "executable_path": "./x64/Debug/REQ_ZoneServer.exe",
    "args": ["--zone_id=1", "--port=7779"]
  }]
}
```

### Multiple Zones (Production)
```json
{
  "auto_launch_zones": true,
  "zones": [
    {
      "zone_id": 10,
      "zone_name": "East Freeport",
      "host": "127.0.0.1",
      "port": 7000,
      "executable_path": "C:/GameServers/REQ/ZoneServer.exe",
      "args": ["--world_id=1", "--zone_id=10", "--port=7000", "--log_level=INFO"]
    },
    {
      "zone_id": 20,
      "zone_name": "North Ro",
      "host": "127.0.0.1",
      "port": 7001,
      "executable_path": "C:/GameServers/REQ/ZoneServer.exe",
      "args": ["--world_id=1", "--zone_id=20", "--port=7001", "--log_level=INFO"]
    }
  ]
}
```

### External Management (Production Cluster)
```json
{
  "auto_launch_zones": false,
  "zones": [
    {
      "zone_id": 10,
      "zone_name": "East Freeport",
      "host": "zone-server-1.internal",
      "port": 7000
    },
    {
      "zone_id": 20,
      "zone_name": "North Ro",
      "host": "zone-server-2.internal",
      "port": 7001
    }
  ]
}
```
In this case, zone servers are managed by container orchestration (Kubernetes, Docker Swarm) or systemd.
