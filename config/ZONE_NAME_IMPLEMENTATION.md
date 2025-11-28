# Zone Name Command-Line Argument Implementation

## Overview
ZoneServer instances now receive and display human-readable zone names (e.g., "East Freeport", "North Ro") alongside numeric zone IDs. This improves log readability and enables better admin diagnostics.

## Changes Summary

### REQ_Shared
**No changes required** - `WorldZoneConfig` already includes `std::string zoneName` field loaded from JSON.

### REQ_WorldServer
**File:** `src/WorldServer.cpp` - `spawnZoneProcess()` method

**Changes:**
- Appends `--zone_name=<zoneName>` argument when spawning zone processes
- Properly quotes zone names containing spaces
- Enhanced logging to show full command line including zone name

**Example Command Line:**
```
./REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7000 "--zone_name=East Freeport"
```

### REQ_ZoneServer

#### ZoneServer.h
**Changes:**
- Updated constructor signature to accept `zoneName` parameter:
  ```cpp
  ZoneServer(std::uint32_t worldId,
             std::uint32_t zoneId,
             const std::string& zoneName,  // NEW
             const std::string& address,
             std::uint16_t port);
  ```
- Added `std::string zoneName_` member variable

#### ZoneServer.cpp
**Changes:**
- Constructor stores `zoneName` parameter in `zoneName_` member
- Enhanced logging to include zone name in quotes:
  ```
  ZoneServer starting: worldId=1, zoneId=10, zoneName="East Freeport", address=0.0.0.0, port=7000
  ```
- All connection and message logs now include zone name for context
- Welcome message uses zone name: `"Welcome to East Freeport (zone 10 on world 1)"`

#### main.cpp
**Changes:**
- Added `--zone_name=<value>` parsing
- Default value: `"UnknownZone"` if argument not provided
- Logs parsed zone name with quotes
- Warns if using default (not provided by auto-launch)
- Passes `zoneName` to `ZoneServer` constructor

## Logging Examples

### WorldServer Auto-Launch
```
[INFO] [world] Processing zone: id=10, name=East Freeport
[INFO] [world]   endpoint=127.0.0.1:7000
[INFO] [world]   executable=./REQ_ZoneServer.exe
[INFO] [world]   args.size()=3
[INFO] [world] Spawning process with full command line:
[INFO] [world]   ./REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7000 "--zone_name=East Freeport"
[INFO] [world] Process created successfully - PID: 12345
[INFO] [world] ? Successfully launched zone 10 (East Freeport)
```

### ZoneServer Startup (Auto-Launched)
```
[INFO] [Main] Parsing 4 command-line argument(s)
[INFO] [Main]   Parsed --world_id=1
[INFO] [Main]   Parsed --zone_id=10
[INFO] [Main]   Parsed --port=7000
[INFO] [Main]   Parsed --zone_name="East Freeport"
[INFO] [Main] Final ZoneServer configuration:
[INFO] [Main]   worldId=1
[INFO] [Main]   zoneId=10
[INFO] [Main]   zoneName="East Freeport"
[INFO] [Main]   address=0.0.0.0
[INFO] [Main]   port=7000
[INFO] [zone] ZoneServer constructed:
[INFO] [zone]   worldId=1
[INFO] [zone]   zoneId=10
[INFO] [zone]   zoneName="East Freeport"
[INFO] [zone]   address=0.0.0.0
[INFO] [zone]   port=7000
[INFO] [zone] ZoneServer starting: worldId=1, zoneId=10, zoneName="East Freeport", address=0.0.0.0, port=7000
```

### ZoneServer with Manual Start (No Zone Name)
```
[INFO] [Main] Parsing 0 command-line argument(s)
[WARN] [Main] Using DEFAULT worldId=1 (--world_id not provided)
[WARN] [Main] Using DEFAULT zoneId=1 (--zone_id not provided)
[WARN] [Main] Using DEFAULT zoneName="UnknownZone" (--zone_name not provided)
[WARN] [Main] Using DEFAULT port=7779 (--port not provided)
```

### Client Connection with Zone Names
```
[INFO] [zone] New client connected to zone "East Freeport" (id=10)
[INFO] [zone] ZoneAuthRequest: handoffToken=987654321, characterId=42 for zone "East Freeport" (id=10)
[INFO] [zone] ZoneAuthResponse OK: characterId=42 entered "East Freeport" (zone 10 on world 1)
```

### TestClient Welcome Message
```
[INFO] [TestClient] Zone entry successful: Welcome to East Freeport (zone 10 on world 1)
```

## Zone Name Quoting Behavior

### Zone Names WITHOUT Spaces
Command line:
```
--zone_name=Freeport
```
No quotes needed - passed as single argument.

### Zone Names WITH Spaces
Command line (quoted):
```
"--zone_name=East Freeport"
```
Entire argument quoted to ensure it's parsed as single token.

### WorldServer Quoting Logic
```cpp
std::string zoneNameArg = "--zone_name=" + zone.zoneName;
if (zone.zoneName.find(' ') != std::string::npos) {
    cmdLineBuilder << "\"" << zoneNameArg << "\"";  // Quote entire arg
} else {
    cmdLineBuilder << zoneNameArg;  // No quotes needed
}
```

## Configuration File Structure

### world_config.json
```json
{
  "world_id": 1,
  "world_name": "CazicThule",
  "zones": [
    {
      "zone_id": 10,
      "zone_name": "East Freeport",     // ? Human-readable name
      "host": "127.0.0.1",
      "port": 7000,
      "executable_path": "./REQ_ZoneServer.exe",
      "args": [
        "--world_id=1",
        "--zone_id=10",
        "--port=7000"
      ]
    },
    {
      "zone_id": 20,
      "zone_name": "North Ro",          // ? Name with space
      "host": "127.0.0.1",
      "port": 7001,
      "executable_path": "./REQ_ZoneServer.exe",
      "args": [
        "--world_id=1",
        "--zone_id=20",
        "--port=7001"
      ]
    }
  ]
}
```

**Note:** The `--zone_name` argument is **automatically appended** by WorldServer during auto-launch. You do NOT need to include it in the `args` array.

## Testing Verification

### 1. Start WorldServer
```bash
cd x64/Debug
.\REQ_WorldServer.exe
```

**Check logs for:**
- Each zone shows `name=<zoneName>` in config loading
- Spawn command includes `"--zone_name=<name>"` with proper quoting

### 2. Observe ZoneServer Consoles
Each auto-launched zone should show:
```
[INFO] [Main] Parsed --zone_name="<actual zone name>"
[INFO] [zone] ZoneServer starting: ... zoneName="<actual zone name>" ...
```

### 3. Run TestClient
```bash
.\REQ_TestClient.exe
```

**Check welcome message:**
```
Zone entry successful: Welcome to East Freeport (zone 10 on world 1)
```

## Benefits

### Improved Log Readability
**Before:**
```
[INFO] [zone] ZoneServer starting: zoneId=10
[INFO] [zone] New client connected
[INFO] [zone] Character 42 entered zone 10
```

**After:**
```
[INFO] [zone] ZoneServer starting: zoneId=10, zoneName="East Freeport"
[INFO] [zone] New client connected to zone "East Freeport" (id=10)
[INFO] [zone] Character 42 entered "East Freeport" (zone 10 on world 1)
```

### Admin Diagnostics
- Quick zone identification in logs without looking up IDs
- Better player support ("Which zone are you in?" ? readable name)
- Easier debugging of zone-specific issues

### Future Features
- Admin commands can reference zones by name
- Zone selection UI can display names
- Logging/monitoring systems can group by zone name
- Analytics dashboards show meaningful zone labels

## Error Handling

### Missing Zone Name Argument
If manually started without `--zone_name`:
```
[WARN] [Main] Using DEFAULT zoneName="UnknownZone" (--zone_name not provided)
```

Zone will function normally but logs will show "UnknownZone".

### Zone Name with Special Characters
Currently supports:
- Spaces (properly quoted)
- Alphanumeric characters
- Common punctuation

**TODO:** Test and handle:
- International characters (UTF-8)
- Quotes within zone names
- Very long zone names

### Empty Zone Name
If config has empty `zone_name`:
```cpp
zone.zoneName = getRequired<std::string>(zoneJson, "zone_name", "Zone entry");
```
This will throw an error during config loading, preventing invalid config.

## Implementation Details

### Constructor Parameter Order
```cpp
ZoneServer(std::uint32_t worldId,    // ID parameters first
           std::uint32_t zoneId,
           const std::string& zoneName,  // Name for display
           const std::string& address,   // Network config last
           std::uint16_t port);
```

Logical grouping:
1. Identity (IDs, name)
2. Network configuration

### Member Variable Storage
```cpp
std::uint32_t worldId_{};     // Numeric ID
std::uint32_t zoneId_{};      // Numeric ID
std::string zoneName_{};      // Human-readable name
std::string address_{};       // Network address
std::uint16_t port_{};        // Network port
```

All stored as members for use in logging and future admin queries.

### Log Message Format
Zone names are consistently quoted in logs for clarity:
```
zoneName="East Freeport"
```

This makes it clear where the name starts/ends, especially with spaces.

## Compatibility

### Backward Compatibility
- Old zone servers (without zone name support) will ignore `--zone_name` argument
- Will log warning about unknown argument
- Will use default "UnknownZone"
- Otherwise functions normally

### Forward Compatibility
- Additional zone metadata can be passed via command-line args
- Future args: `--zone_type`, `--zone_level_range`, `--zone_difficulty`
- Same parsing infrastructure supports any `--key=value` format

## Known Limitations

### 1. Zone Name Uniqueness Not Enforced
- Multiple zones can have same name
- Identified by ID, not name
- Admin queries should use ID for precision

### 2. No Zone Name Validation
- Doesn't check for profanity, length limits
- Accepts any string from config
- **TODO:** Add validation rules

### 3. Platform-Specific Quoting
- Current implementation is Windows-specific
- Linux/macOS will need different quoting logic
- **TODO:** Implement cross-platform spawn

## Future Enhancements

### 1. Zone Name in Database
- Store zone names in central database
- ZoneServer can query by ID on startup
- Eliminates need for command-line passing

### 2. Localization Support
- Store multiple language names
- Select language based on client locale
- Show translated names in logs/UI

### 3. Dynamic Zone Renaming
- Admin command to rename zone at runtime
- Broadcast name change to connected clients
- Update logs without restart

### 4. Zone Name in Protocol
- Include zone name in ZoneAuthResponse
- Client displays name in UI
- Reduces client-side zone ID lookups

## Summary

**Status:** ? Fully Implemented and Tested

**What Changed:**
1. WorldServer passes `--zone_name` argument with proper quoting
2. ZoneServer parses and stores zone name
3. All logs include human-readable zone names
4. Welcome messages show zone name to players

**Expected Behavior:**
- Each auto-launched zone displays its configured name
- Logs clearly show which zone is which
- Manual starts without name show "UnknownZone" warning
- Spaces in names handled correctly with quotes

**Testing Results:**
- Build: Successful
- Auto-launch: Zone names appear in logs
- Manual start: Default warning works
- Client connection: Welcome shows zone name

**Next Steps:**
- Monitor logs for any quoting issues with special characters
- Consider adding zone name to client protocol
- Plan database integration for zone metadata
