# REQ Backend Configuration Files

This directory contains JSON configuration files for the REQ backend servers.

## Login Server Configuration (login_config.json)

```json
{
  "address": "0.0.0.0",      // Listen address (0.0.0.0 for all interfaces)
  "port": 7777,              // Listen port
  "motd": "Welcome message"  // Message of the day (optional)
}
```

### Fields:
- `address` (string, optional): IP address to bind to. Default: "0.0.0.0"
- `port` (number, optional): TCP port to listen on. Default: 7777. Must be 1-65535.
- `motd` (string, optional): Message of the day displayed on login. Default: empty

## World Server Configuration (world_config.json)

```json
{
  "world_id": 1,                  // Unique world identifier
  "world_name": "MainWorld",      // Display name
  "address": "0.0.0.0",          // Listen address
  "port": 7778,                  // Listen port
  "ruleset_id": "standard",      // Ruleset identifier (e.g., "standard", "pvp", "rp")
  "auto_launch_zones": false,    // Whether to auto-launch zone servers
  "zones": [                     // Array of zone configurations
    {
      "zone_id": 1,
      "zone_name": "Elwynn Forest",
      "host": "127.0.0.1",
      "port": 7779,
      "executable_path": "",     // Optional: path to zone server executable (for auto-launch)
      "args": []                 // Optional: command-line arguments (for auto-launch)
    }
  ]
}
```

### Required Fields:
- `world_id` (number): Unique identifier for this world server
- `world_name` (string): Human-readable name shown in world list
- `address` (string): IP address to bind to
- `port` (number): TCP port (1-65535)
- `ruleset_id` (string): Identifier for game rules/variant
- `zones` (array): At least one zone must be defined

### Zone Configuration:
Each zone in the `zones` array requires:
- `zone_id` (number): Unique zone identifier
- `zone_name` (string): Human-readable zone name
- `host` (string): Hostname/IP where zone server runs
- `port` (number): Zone server port (1-65535)

Optional fields for auto-launch mode:
- `executable_path` (string): Path to zone server executable
- `args` (array of strings): Command-line arguments

### Auto-Launch Mode:
When `auto_launch_zones` is `true`, the world server will attempt to automatically
start zone servers using the `executable_path` and `args` specified for each zone.
This is useful for development and single-machine deployments.

## Error Handling

All configuration files use strict validation:
- Missing required fields will cause startup failure
- Invalid port ranges (0 or >65535) will be rejected
- Parse errors in JSON syntax will be logged with details
- Type mismatches (e.g., string where number expected) will fail

Check console/log output for detailed error messages if a server fails to start.

## Examples

See the example files in this directory:
- `login_config.json` - Basic login server config
- `world_config.json` - Multi-zone world server
- `world_config_autolaunch_example.json` - World with auto-launched zones
