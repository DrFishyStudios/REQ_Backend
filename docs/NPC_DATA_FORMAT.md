# NPC Data Format Specification

## Overview

This document defines the JSON data formats for NPC templates and spawn points in REQ_Backend. These files are loaded at zone startup to populate the world with NPCs.

---

## File Locations

- **NPC Templates:** `config/npc_templates.json` (global, shared across all zones)
- **Zone Spawns:** `config/zones/npc_spawns_<zone_id>.json` (per-zone spawn tables)

---

## 1. NPC Templates (`npc_templates.json`)

### Purpose
Defines reusable NPC archetypes that can be instantiated at spawn points. Each template contains stats, behavior flags, and references to visual assets.

### Schema

```json
{
  "templates": [
    {
      "npc_id": <int>,
      "name": <string>,
      "level": <int>,
      "archetype": <string>,
      
      "hp": <int>,
      "ac": <int>,
      "min_damage": <int>,
      "max_damage": <int>,
      
      "faction_id": <int>,
      "loot_table_id": <int>,
      "visual_id": <int or string>,
      
      "is_social": <bool>,
      "can_flee": <bool>,
      "is_roamer": <bool>,
      
      "aggro_radius": <float>,
      "assist_radius": <float>
    }
  ]
}
```

### Field Definitions

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `npc_id` | int | Yes | - | Unique identifier for this NPC template |
| `name` | string | Yes | - | Display name (e.g., "A Decaying Skeleton") |
| `level` | int | Yes | 1 | NPC level for combat calculations |
| `archetype` | string | No | "melee_trash" | Behavior archetype: "melee_trash", "caster_trash", "named", "boss", etc. |
| `hp` | int | Yes | 100 | Maximum hit points |
| `ac` | int | No | 10 | Armor class (mitigation) |
| `min_damage` | int | No | 1 | Minimum melee damage |
| `max_damage` | int | No | 5 | Maximum melee damage |
| `faction_id` | int | No | 0 | Faction identifier (0 = none) |
| `loot_table_id` | int | No | 0 | Loot table reference (0 = none) |
| `visual_id` | int/string | No | 0 | Client-side visual asset reference |
| `is_social` | bool | No | false | Whether NPC assists nearby allies |
| `can_flee` | bool | No | false | Whether NPC flees at low HP |
| `is_roamer` | bool | No | false | Whether NPC wanders from spawn point |
| `aggro_radius` | float | No | 10.0 | Proximity aggro range (game units) |
| `assist_radius` | float | No | 15.0 | Social aggro range (game units) |

### Example

```json
{
  "templates": [
    {
      "npc_id": 1001,
      "name": "A Decaying Skeleton",
      "level": 1,
      "archetype": "melee_trash",
      
      "hp": 20,
      "ac": 10,
      "min_damage": 1,
      "max_damage": 3,
      
      "faction_id": 100,
      "loot_table_id": 10,
      "visual_id": "skeleton_basic",
      
      "is_social": true,
      "can_flee": false,
      "is_roamer": false,
      
      "aggro_radius": 8.0,
      "assist_radius": 12.0
    },
    {
      "npc_id": 1002,
      "name": "A Rat",
      "level": 1,
      "archetype": "critter",
      
      "hp": 10,
      "ac": 5,
      "min_damage": 1,
      "max_damage": 2,
      
      "faction_id": 101,
      "loot_table_id": 11,
      "visual_id": "rat_basic",
      
      "is_social": false,
      "can_flee": true,
      "is_roamer": true,
      
      "aggro_radius": 5.0,
      "assist_radius": 0.0
    }
  ]
}
```

---

## 2. Zone Spawn Points (`npc_spawns_<zone_id>.json`)

### Purpose
Defines physical spawn locations for NPCs within a specific zone. Each spawn point references an NPC template and includes position, respawn timing, and optional behaviors.

### Schema

```json
{
  "zone_id": <int>,
  "spawns": [
    {
      "spawn_id": <int>,
      "npc_id": <int>,
      
      "position": {
        "x": <float>,
        "y": <float>,
        "z": <float>
      },
      
      "heading": <float>,
      
      "respawn_seconds": <int>,
      "respawn_variance_seconds": <int>,
      
      "spawn_group": <string>
    }
  ]
}
```

### Field Definitions

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `zone_id` | int | Yes | - | Zone identifier this spawn table belongs to |
| `spawn_id` | int | Yes | - | Unique identifier for this spawn point |
| `npc_id` | int | Yes | - | Reference to NPC template ID |
| `position.x` | float | Yes | 0.0 | World X coordinate |
| `position.y` | float | Yes | 0.0 | World Y coordinate |
| `position.z` | float | Yes | 0.0 | World Z coordinate (height) |
| `heading` | float | No | 0.0 | Facing direction in degrees (0-360) |
| `respawn_seconds` | int | No | 120 | Base respawn time in seconds |
| `respawn_variance_seconds` | int | No | 0 | Random variance (±) added to respawn time |
| `spawn_group` | string | No | "" | Logical grouping for camps/encounters |

### Example

```json
{
  "zone_id": 10,
  "spawns": [
    {
      "spawn_id": 1,
      "npc_id": 1001,
      
      "position": {
        "x": 100.0,
        "y": 50.0,
        "z": 0.0
      },
      
      "heading": 0.0,
      
      "respawn_seconds": 120,
      "respawn_variance_seconds": 30,
      
      "spawn_group": "skeleton_camp_1"
    },
    {
      "spawn_id": 2,
      "npc_id": 1001,
      
      "position": {
        "x": 150.0,
        "y": 60.0,
        "z": 0.0
      },
      
      "heading": 90.0,
      
      "respawn_seconds": 120,
      "respawn_variance_seconds": 30,
      
      "spawn_group": "skeleton_camp_1"
    },
    {
      "spawn_id": 3,
      "npc_id": 1002,
      
      "position": {
        "x": 200.0,
        "y": 75.0,
        "z": 0.0
      },
      
      "heading": 45.0,
      
      "respawn_seconds": 90,
      "respawn_variance_seconds": 15,
      
      "spawn_group": "rat_spawns"
    }
  ]
}
```

---

## Validation Rules

### NPC Templates
- `npc_id` must be unique across all templates
- `npc_id` must be > 0
- `name` must not be empty
- `level` must be >= 1
- `hp` must be > 0
- `min_damage` <= `max_damage`
- `aggro_radius` >= 0.0
- `assist_radius` >= 0.0

### Spawn Points
- `spawn_id` must be unique within the zone
- `spawn_id` must be > 0
- `npc_id` must reference a valid NPC template
- `zone_id` must match the zone loading this file
- `respawn_seconds` must be >= 0
- `respawn_variance_seconds` must be >= 0
- `heading` should be 0.0-360.0 (not enforced, will wrap)

---

## Integration Notes

### Loading Order
1. Load `npc_templates.json` first (global templates)
2. Load `npc_spawns_<zone_id>.json` for each zone
3. Validate all `npc_id` references exist in templates
4. Instantiate NPCs at spawn points

### Error Handling
- Missing template file: **Fatal error**, zone cannot start
- Missing spawn file: **Warning**, zone starts with no NPCs
- Invalid `npc_id` reference: **Warning**, skip that spawn point
- Malformed JSON: **Fatal error**, zone cannot start
- Duplicate IDs: **Warning**, use first occurrence

### Future Extensions
- Spawn groups with weighted selection
- Placeholder/named spawn logic
- Day/night spawn rules
- Dynamic spawn density
- Roaming paths (waypoint lists)

---

## Example: Newbie Camp

### NPC Templates
```json
{
  "templates": [
    {
      "npc_id": 1001,
      "name": "A Decaying Skeleton",
      "level": 1,
      "archetype": "melee_trash",
      "hp": 20,
      "ac": 10,
      "min_damage": 1,
      "max_damage": 3,
      "faction_id": 100,
      "loot_table_id": 10,
      "visual_id": "skeleton_basic",
      "is_social": true,
      "can_flee": false,
      "is_roamer": false,
      "aggro_radius": 8.0,
      "assist_radius": 12.0
    },
    {
      "npc_id": 1002,
      "name": "A Rat",
      "level": 1,
      "archetype": "critter",
      "hp": 10,
      "ac": 5,
      "min_damage": 1,
      "max_damage": 2,
      "faction_id": 101,
      "loot_table_id": 11,
      "visual_id": "rat_basic",
      "is_social": false,
      "can_flee": true,
      "is_roamer": true,
      "aggro_radius": 5.0,
      "assist_radius": 0.0
    },
    {
      "npc_id": 1003,
      "name": "A Fire Beetle",
      "level": 2,
      "archetype": "melee_trash",
      "hp": 30,
      "ac": 12,
      "min_damage": 2,
      "max_damage": 5,
      "faction_id": 101,
      "loot_table_id": 12,
      "visual_id": "beetle_fire",
      "is_social": false,
      "can_flee": false,
      "is_roamer": false,
      "aggro_radius": 6.0,
      "assist_radius": 0.0
    }
  ]
}
```

### Zone 10 Spawns
```json
{
  "zone_id": 10,
  "spawns": [
    {
      "spawn_id": 1,
      "npc_id": 1001,
      "position": { "x": 100.0, "y": 50.0, "z": 0.0 },
      "heading": 0.0,
      "respawn_seconds": 120,
      "respawn_variance_seconds": 30,
      "spawn_group": "skeleton_camp"
    },
    {
      "spawn_id": 2,
      "npc_id": 1001,
      "position": { "x": 150.0, "y": 60.0, "z": 0.0 },
      "heading": 90.0,
      "respawn_seconds": 120,
      "respawn_variance_seconds": 30,
      "spawn_group": "skeleton_camp"
    },
    {
      "spawn_id": 3,
      "npc_id": 1002,
      "position": { "x": 200.0, "y": 75.0, "z": 0.0 },
      "heading": 45.0,
      "respawn_seconds": 90,
      "respawn_variance_seconds": 15,
      "spawn_group": "rat_spawns"
    },
    {
      "spawn_id": 4,
      "npc_id": 1003,
      "position": { "x": 175.0, "y": 120.0, "z": 0.0 },
      "heading": 180.0,
      "respawn_seconds": 180,
      "respawn_variance_seconds": 45,
      "spawn_group": "beetle_spawns"
    }
  ]
}
```

This creates a simple newbie camp with:
- 2 skeleton spawns (social, will assist each other)
- 1 rat spawn (roamer, will flee)
- 1 beetle spawn (static, higher HP)

---

## Change Log

- **2025-01-19:** Initial specification
  - Defined NPC template and spawn point schemas
  - Added validation rules and examples
