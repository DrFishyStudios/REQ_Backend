# NPC Runtime Instantiation - Usage Guide

## Overview

This system instantiates NPCs from templates and spawn points at zone startup, creating fully functional combat-ready entities that:
- Spawn at configured locations
- Engage players via aggro system
- Attack with melee combat
- Leash back to spawn when pulled too far
- Die and respawn on timers
- Award XP on death

---

## System Architecture

### Data Flow

```
Zone Startup
    ?
    ?
Load NPC Templates
(config/npc_templates.json)
    ?
    ?
Load Spawn Points  
(config/zones/npc_spawns_<zoneId>.json)
    ?
    ?
Instantiate NPCs
- Create ZoneNpc from template
- Position at spawn point
- Add to npcs_ map
    ?
    ?
Simulation Loop (20Hz)
- Update AI state machine
- Handle aggro/combat
- Process respawns
```

### Components

1. **NPC Templates** (`config/npc_templates.json`)
   - Reusable NPC definitions
   - Stats, behavior flags, combat parameters

2. **Spawn Points** (`config/zones/npc_spawns_<zoneId>.json`)
   - Where NPCs spawn in the zone
   - Respawn timers
   - References template IDs

3. **NPC Instance** (`ZoneNpc` struct)
   - Runtime entity with unique ID
   - Position, HP, AI state
   - Hate table, combat timers

4. **AI System** (`updateNpcAi()`)
   - State machine: Idle ? Alert ? Engaged ? Leashing/Dead
   - Proximity aggro, damage aggro, social aggro
   - Leashing, fleeing behaviors

5. **Respawn System** (`updateNpc()`)
   - Death timer countdown
   - Respawn at spawn point
   - Full HP restoration

---

## Starting a Zone Server with NPCs

### 1. Verify Configuration Files Exist

**NPC Templates:**
```bash
config/npc_templates.json
```

**Spawn Points for Zone 10:**
```bash
config/zones/npc_spawns_10.json
```

### 2. Start Zone Server

```bash
cd x64/Debug
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="East Freeport"
```

### 3. Check Logs for NPC Loading

**Expected output:**
```
[INFO] [zone] === Loading NPC Data ===
[INFO] [zone] Successfully loaded 6 NPC template(s)
[INFO] [zone] Successfully loaded 6 spawn point(s)
[INFO] [zone] [SPAWN] === Instantiating NPCs from spawn data ===
[INFO] [zone] [SPAWN] Spawned NPC: instanceId=1, templateId=1001, name="A Decaying Skeleton", level=1, spawnId=1, pos=(100,50,0)
[INFO] [zone] [SPAWN] Spawned NPC: instanceId=2, templateId=1002, name="A Giant Rat", level=1, spawnId=2, pos=(200,75,0)
...
[INFO] [zone] [SPAWN] Instantiated 6 NPC(s) from 6 spawn point(s)
```

---

## NPC Lifecycle

### 1. Spawn (Zone Startup)

```
[SPAWN] Spawned NPC: instanceId=1, templateId=1001, name="A Decaying Skeleton"
```

**What happens:**
- NPC created from template
- Positioned at spawn point
- HP set to max
- AI state: Idle
- Added to zone's npcs_ map with unique instanceId

### 2. Idle State

```
[NPC] Active: 6 NPC(s) - Idle:6, Alert:0, Engaged:0, Leashing:0, Fleeing:0, Dead:0
```

**What happens:**
- NPC stands at spawn point
- Scans for players every 0.5-1.0 seconds
- Checks if player within aggroRadius
- If player detected ? Alert state

### 3. Aggro (Proximity or Attack)

**Proximity aggro:**
```
[AI] NPC 1 "A Decaying Skeleton" state=Idle->Alert (proximity aggro), target=42, distance=7.5
[HATE] NPC 1 new_target=42 top_hate=1.0
[AI] NPC 1 state=Alert->Engaged
```

**Attack aggro:**
```
[AI] NPC 1 "A Decaying Skeleton" state->Engaged (damage aggro), attacker=42, damage=15
```

**What happens:**
- NPC adds hate for player
- Transitions: Idle ? Alert ? Engaged
- Begins chasing player

### 4. Combat

```
[COMBAT] NPC 1 "A Decaying Skeleton" melee attack, target=42, damage=3, targetHp=97/100
```

**What happens:**
- NPC chases player at moveSpeed (50 units/sec)
- When in melee range (20 units): attacks every 1.5 seconds
- Rolls damage between minDamage and maxDamage
- Applies damage to player HP
- Checks if player died

### 5. Leashing

**Triggered when:**
- Distance from spawn > leashRadius (200 units)
- Distance to target > maxChaseDistance (250 units)
- Target dies or disconnects

```
[AI] NPC 1 state=Engaged->Leashing (exceeded leash), distFromSpawn=235.7
[HATE] Cleared hate for NPC 1
[AI] NPC 1 state=Leashing->Idle (reached spawn, reset)
```

**What happens:**
- NPC runs back to spawn point
- HP restored to full
- Hate table cleared
- Returns to Idle state

### 6. Death

**When player kills NPC:**
```
[COMBAT] NPC slain: npcId=1, name="A Decaying Skeleton", killerCharId=42
[NPC] NPC died, respawn in 120s: id=1, name="A Decaying Skeleton"
```

**What happens:**
- NPC HP reaches 0
- isAlive = false
- AI state: Dead
- XP awarded to killer
- Respawn timer starts

### 7. Respawn

**After timer expires:**
```
[NPC] Respawned: id=1, name="A Decaying Skeleton", pos=(100,50,0)
```

**What happens:**
- NPC teleported back to spawn point
- HP restored to full
- isAlive = true
- AI state: Idle
- Hate table cleared
- Ready to aggro again

---

## NPC Behaviors

### Proximity Aggro

**Range:** Defined by `aggroRadius` in template (default 80 GDD units = 8.0 server units)

**How it works:**
- NPC scans for players every 0.5-1.0 seconds (low-frequency)
- If player within aggroRadius: adds initial hate and engages
- Only scans when in Idle state

### Damage Aggro

**How it works:**
- Player attacks NPC with AttackRequest
- NPC adds hate based on damage dealt (hate = damage * 1.0)
- NPC switches target if new attacker deals more damage
- Works even if NPC is outside proximity aggro range

### Social Aggro

**Condition:** `isSocial = true` in template

**How it works:**
- When NPC A engages a player
- All nearby NPCs with same faction within socialRadius assist
- Assisting NPCs add 0.5x hate and enter Alert state

```
[AI] Social assist: NPC 2 "A Giant Rat" assisting NPC 1, distance=45.3
```

### Leashing

**Triggers:**
- NPC pulled > leashRadius from spawn (default 200 units)
- Target moves > maxChaseDistance away (default 250 units)

**Behavior:**
- NPC runs back to spawn at normal speed
- Heals to full HP on reaching spawn
- Clears hate and returns to Idle

### Fleeing

**Condition:** `canFlee = true` in template

**Triggers:** HP drops below fleeHealthPercent (default 25%)

**Behavior:**
- NPC runs away from attacker
- Switches to Leashing when far enough from spawn
- Eventually returns to spawn and heals

---

## Combat System Integration

### Player Attacks NPC

**Client sends:**
```
MessageType: AttackRequest
Payload: "42|1|0|1"  (characterId 42 attacks NPC instanceId 1, basic attack)
```

**Server processes:**
1. Validates range (max 200 units)
2. Calculates hit chance (95%)
3. Calculates damage (level + strength/10 + variance)
4. Applies damage to NPC HP
5. Adds hate to NPC
6. Triggers aggro if Idle/Alert
7. Checks if NPC died
8. Awards XP if killed
9. Broadcasts AttackResult

**Server responds:**
```
MessageType: AttackResult
Payload: "42|1|15|1|5|0|You hit A Decaying Skeleton for 15 damage"
```

### NPC Attacks Player

**Triggered by:** updateNpcAi() in Engaged state

**Process:**
1. NPC in melee range (20 units)
2. Attack cooldown ready (every 1.5 seconds)
3. Rolls damage between minDamage and maxDamage
4. Applies damage to player HP
5. Marks player combatStatsDirty
6. Checks if player died
7. Logs combat event

**Example:**
```
[COMBAT] NPC 1 "A Decaying Skeleton" melee attack, target=42, damage=3, targetHp=97/100
```

---

## Adding New NPCs

### 1. Add NPC Template

**Edit:** `config/npc_templates.json`

```json
{
  "npc_id": 1007,
  "name": "A Skeleton Warrior",
  "level": 3,
  "hp": 50,
  "min_damage": 3,
  "max_damage": 7,
  "faction_id": 1,
  "is_social": true,
  "can_flee": false,
  "aggro_radius": 10.0,
  "assist_radius": 8.0
}
```

### 2. Add Spawn Point

**Edit:** `config/zones/npc_spawns_10.json`

```json
{
  "spawn_id": 7,
  "npc_id": 1007,
  "pos_x": 400.0,
  "pos_y": 200.0,
  "pos_z": 0.0,
  "heading": 0.0,
  "respawn_seconds": 180
}
```

### 3. Restart Zone Server

```bash
cd x64/Debug
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="East Freeport"
```

### 4. Verify in Logs

```
[SPAWN] Spawned NPC: instanceId=7, templateId=1007, name="A Skeleton Warrior", level=3
```

---

## Monitoring NPCs

### Real-Time State Summary

**Every 5 seconds:**
```
[NPC] Active: 6 NPC(s) - Idle:4, Alert:0, Engaged:2, Leashing:0, Fleeing:0, Dead:0
```

**Shows:**
- Total NPCs in zone
- Breakdown by AI state

### Grep Patterns for Logs

**All NPC events:**
```bash
grep "\[NPC\]\|\[AI\]\|\[SPAWN\]\|\[HATE\]" zone_server.log
```

**Spawns only:**
```bash
grep "\[SPAWN\]" zone_server.log
```

**Combat events:**
```bash
grep "\[COMBAT\]" zone_server.log
```

**AI state changes:**
```bash
grep "\[AI\]" zone_server.log
```

**Hate/aggro:**
```bash
grep "\[HATE\]" zone_server.log
```

**Respawns:**
```bash
grep "Respawned" zone_server.log
```

---

## Troubleshooting

### NPCs Not Spawning

**Check logs for:**
```
[WARN] [zone] Failed to load NPC templates
[WARN] [zone] Failed to load zone spawns
```

**Verify files exist:**
```bash
ls config/npc_templates.json
ls config/zones/npc_spawns_10.json
```

**Check JSON syntax:**
- Use JSON validator
- Look for missing commas, brackets
- Ensure valid field types

### NPCs Not Aggroing

**Check:**
- Player within aggroRadius (default 8.0 units)
- NPC in Idle state (not Dead/Leashing)
- Player not dead

**Logs to check:**
```
grep "proximity aggro" zone_server.log
```

### NPCs Not Attacking

**Check:**
- NPC in Engaged state
- Player within melee range (20 units)
- Attack cooldown expired (default 1.5s)

**Logs to check:**
```
grep "melee attack" zone_server.log
```

### NPCs Not Respawning

**Check:**
- NPC died (isAlive = false)
- Respawn timer started
- Timer countdown in progress

**Logs to check:**
```
grep "NPC died, respawn in" zone_server.log
grep "Respawned" zone_server.log
```

---

## Performance Considerations

### Spawn Limits

**Current system handles:**
- ~100 NPCs per zone efficiently
- 20Hz tick rate (50ms per tick)
- O(N*M) aggro scan (N=NPCs, M=players)

**For larger zones:**
- Consider spatial partitioning
- Reduce aggro scan frequency
- Implement interest management

### Memory Usage

**Per NPC:**
- ~300 bytes (ZoneNpc struct)
- 100 NPCs = ~30KB (negligible)

**No dynamic allocations per tick**

---

## Configuration Reference

### NPC Template Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| npc_id | int | required | Unique template ID |
| name | string | required | Display name |
| level | int | required | NPC level |
| hp | int | required | Maximum HP |
| min_damage | int | 1 | Minimum melee damage |
| max_damage | int | 5 | Maximum melee damage |
| faction_id | int | 0 | Faction for social aggro |
| is_social | bool | false | Assists allies in combat |
| can_flee | bool | false | Runs away at low HP |
| aggro_radius | float | 8.0 | Proximity aggro range (units) |
| assist_radius | float | 6.0 | Social aggro range (units) |

### Spawn Point Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| spawn_id | int | required | Unique spawn ID |
| npc_id | int | required | Template ID to spawn |
| pos_x | float | required | X coordinate |
| pos_y | float | required | Y coordinate |
| pos_z | float | 0.0 | Z coordinate (height) |
| heading | float | 0.0 | Facing direction (degrees) |
| respawn_seconds | int | 120 | Respawn timer (seconds) |

### Behavior Constants

```cpp
MELEE_RANGE = 20.0f         // Attack distance
AGGRO_SCAN_INTERVAL = 0.5-1.0s  // Idle scan frequency
LEASH_RADIUS = 200.0f       // Default leash distance
MAX_CHASE_DISTANCE = 250.0f // Max pursuit distance
FLEE_HEALTH_PERCENT = 0.25f // HP threshold for fleeing
MELEE_ATTACK_COOLDOWN = 1.5f // Seconds between attacks
MOVE_SPEED = 50.0f          // Movement speed (units/sec)
```

---

## Future Enhancements

### Planned Features
- **Spawn groups:** Weighted random NPC selection
- **Named/rare spawns:** Boss variants with placeholders
- **Loot tables:** Item drops on death
- **Roaming:** Patrol paths and wandering
- **Ranged combat:** Bow/caster NPCs
- **Special abilities:** Casts, procs, AOE attacks

### Extension Points
- `NpcTemplate.abilityPackageId`: Load spell/ability sets
- `NpcTemplate.navigationPackageId`: Pathfinding behaviors
- `NpcTemplate.visualId`: Client-side model selection
- `SpawnPoint.namedChance`: Rare spawn replacement

---

## Testing Checklist

### Zone Startup
- [ ] NPC templates loaded successfully
- [ ] Spawn points loaded successfully
- [ ] NPCs instantiated at correct positions
- [ ] NPC count matches spawn point count

### AI Behaviors
- [ ] Proximity aggro triggers
- [ ] Damage aggro triggers
- [ ] Social aggro triggers (if isSocial)
- [ ] Leashing works when pulled too far
- [ ] Fleeing works at low HP (if canFlee)

### Combat
- [ ] Player can attack NPC
- [ ] Damage applied correctly
- [ ] NPC counter-attacks in melee range
- [ ] NPC dies at 0 HP
- [ ] XP awarded on kill

### Respawn
- [ ] Death timer starts
- [ ] Timer counts down
- [ ] NPC respawns at spawn point
- [ ] HP restored to full
- [ ] Hate table cleared

---

## Summary

The NPC runtime instantiation system provides a complete, EverQuest-style spawn and AI system:

? **Data-driven** - NPCs defined in JSON templates  
? **Reusable** - Spawn same template at multiple locations  
? **Combat-ready** - Full aggro, melee, death, and respawn  
? **Performant** - Handles ~100 NPCs at 20Hz efficiently  
? **Extensible** - Ready for loot, abilities, and roaming  

**Start testing with Zone 10 (East Freeport) which has 6 sample NPCs!**
