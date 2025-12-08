# NPC Spawn Data System - Test Harness

## Quick Test Commands

### 1. Verify JSON File Locations

```bash
# Check if example files exist
ls -la config/npc_templates.json
ls -la config/zones/npc_spawns_10.json
```

**Expected Output:**
```
-rw-r--r-- 1 user user 2048 Jan 19 12:00 config/npc_templates.json
-rw-r--r-- 1 user user 1024 Jan 19 12:00 config/zones/npc_spawns_10.json
```

### 2. Validate JSON Syntax

```bash
# Use Python to validate JSON
python -m json.tool config/npc_templates.json > /dev/null && echo "Valid JSON" || echo "Invalid JSON"
python -m json.tool config/zones/npc_spawns_10.json > /dev/null && echo "Valid JSON" || echo "Invalid JSON"
```

**Expected Output:**
```
Valid JSON
Valid JSON
```

### 3. Start ZoneServer and Check Logs

```bash
cd x64/Debug
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7000 --zone_name="Test Zone"
```

**Expected Log Output:**
```
[INFO] [zone] ZoneServer starting: worldId=1, zoneId=10, zoneName="Test Zone"
[INFO] [zone] === Loading NPC Data ===
[INFO] [NpcDataRepository] Loading NPC templates from: config/npc_templates.json
[INFO] [NpcDataRepository]   Loaded NPC template: id=1001, name="A Decaying Skeleton", level=1, hp=20, archetype=melee_trash
[INFO] [NpcDataRepository]   Loaded NPC template: id=1002, name="A Rat", level=1, hp=10, archetype=critter
[INFO] [NpcDataRepository]   Loaded NPC template: id=1003, name="A Fire Beetle", level=2, hp=30, archetype=melee_trash
[INFO] [NpcDataRepository]   Loaded NPC template: id=1004, name="A Snake", level=1, hp=15, archetype=critter
[INFO] [NpcDataRepository]   Loaded NPC template: id=1005, name="A Zombie", level=3, hp=50, archetype=melee_trash
[INFO] [NpcDataRepository] Loaded 5 NPC template(s)
[INFO] [zone] Successfully loaded 5 NPC template(s)
[INFO] [NpcDataRepository] Loading zone spawns from: config/zones/npc_spawns_10.json
[INFO] [NpcDataRepository]   Loaded spawn: id=1, npc_id=1001 (A Decaying Skeleton), pos=(100,50,0), respawn=120s
[INFO] [NpcDataRepository]   Loaded spawn: id=2, npc_id=1001 (A Decaying Skeleton), pos=(150,60,0), respawn=120s, group=skeleton_camp
[INFO] [NpcDataRepository]   Loaded spawn: id=3, npc_id=1002 (A Rat), pos=(200,75,0), respawn=90s, group=rat_spawns
[INFO] [NpcDataRepository]   Loaded spawn: id=4, npc_id=1003 (A Fire Beetle), pos=(175,120,0), respawn=180s, group=beetle_spawns
[INFO] [NpcDataRepository]   Loaded spawn: id=5, npc_id=1004 (A Snake), pos=(50,200,0), respawn=90s, group=snake_spawns
[INFO] [NpcDataRepository] Loaded 5 spawn point(s) for zone 10
[INFO] [zone] Successfully loaded 5 spawn point(s)
[INFO] [zone] Simulation tick loop started
```

## Test Scenarios

### Test 1: Normal Load (Success)

**Setup:**
- `config/npc_templates.json` exists with valid data
- `config/zones/npc_spawns_10.json` exists with valid data

**Expected Result:**
- All templates loaded
- All spawn points loaded
- No errors or warnings

### Test 2: Missing Templates File

**Setup:**
- Delete or rename `config/npc_templates.json`

**Expected Log:**
```
[ERROR] [NpcDataRepository] Failed to open NPC templates file: config/npc_templates.json
[WARN] [zone] Failed to load NPC templates - zone will have no NPCs
```

**Expected Result:**
- Zone starts but has no NPCs

### Test 3: Missing Spawns File

**Setup:**
- Templates exist
- `config/zones/npc_spawns_10.json` missing

**Expected Log:**
```
[WARN] [NpcDataRepository] Zone spawn file not found: config/zones/npc_spawns_10.json (zone will have no NPCs)
[WARN] [zone] Failed to load zone spawns - zone will have no NPCs
```

**Expected Result:**
- Zone starts but has no spawn points

### Test 4: Invalid JSON Syntax

**Setup:**
- Corrupt JSON file (remove a closing brace)

**Expected Log:**
```
[ERROR] [NpcDataRepository] Failed to parse JSON from config/npc_templates.json: [json.exception.parse_error.101] parse error at line 5, column 1: syntax error while parsing object - unexpected end of input; expected '}'
```

**Expected Result:**
- Zone fails to start or starts without NPCs

### Test 5: Invalid NPC ID Reference

**Setup:**
- Spawn file references `npc_id: 9999` (doesn't exist)

**Expected Log:**
```
[WARN] [NpcDataRepository] Spawn 6 references non-existent npc_id=9999, skipping
[INFO] [NpcDataRepository] Loaded 5 spawn point(s) for zone 10 (1 skipped)
```

**Expected Result:**
- Zone loads other valid spawns
- Invalid spawn is skipped

### Test 6: Duplicate IDs

**Setup:**
- Two templates with same `npc_id`

**Expected Log:**
```
[WARN] [NpcDataRepository] Duplicate npc_id=1001, skipping
[INFO] [NpcDataRepository] Loaded 4 NPC template(s) (1 skipped)
```

**Expected Result:**
- First occurrence used
- Second occurrence skipped

## Validation Checklist

| Check | Pass | Notes |
|-------|------|-------|
| Templates file loads | ? | 5 NPCs loaded |
| Spawns file loads | ? | 5 spawn points loaded |
| All template fields present | ? | id, name, level, hp, etc. |
| All spawn fields present | ? | spawn_id, npc_id, position, etc. |
| NPC ID references valid | ? | All spawns reference existing NPCs |
| No duplicate template IDs | ? | Each NPC has unique ID |
| No duplicate spawn IDs | ? | Each spawn has unique ID |
| Logging is clear | ? | Easy to read and understand |

## Manual Verification

### Check Loaded Templates

**C++ Code Snippet:**
```cpp
// In ZoneServer after loading
for (const auto& [id, tmpl] : npcDataRepository_.GetAllTemplates()) {
    req::shared::logInfo("zone", std::string{"Template "} + std::to_string(id) + 
        ": " + tmpl.name + " (level " + std::to_string(tmpl.level) + ")");
}
```

### Check Loaded Spawns

**C++ Code Snippet:**
```cpp
// In ZoneServer after loading
for (const auto& spawn : npcDataRepository_.GetZoneSpawns()) {
    const auto* tmpl = npcDataRepository_.GetTemplate(spawn.npcId);
    std::string name = tmpl ? tmpl->name : "Unknown";
    req::shared::logInfo("zone", std::string{"Spawn "} + std::to_string(spawn.spawnId) + 
        " at (" + std::to_string(spawn.posX) + "," + std::to_string(spawn.posY) + "): " + name);
}
```

## Performance Testing

### Load Time Measurement

**Expected:**
- 100 templates: < 50ms
- 1000 spawn points: < 100ms
- 10000 spawn points: < 500ms

**Test:**
```cpp
auto start = std::chrono::high_resolution_clock::now();
npcDataRepository_.LoadNpcTemplates("config/npc_templates.json");
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
req::shared::logInfo("zone", std::string{"Template load time: "} + std::to_string(duration) + "ms");
```

### Memory Usage

**Expected:**
- ~200 bytes per template
- ~100 bytes per spawn point
- Example: 100 templates + 1000 spawns = ~120KB

## Error Recovery Testing

### Test 1: Partial Load Failure

**Setup:**
- 5 valid templates, 1 invalid (missing required field)

**Expected:**
- 5 templates loaded
- 1 skipped with warning

### Test 2: Empty Files

**Setup:**
- JSON files with empty arrays

**Expected:**
```
[WARN] [NpcDataRepository] NPC templates file contains empty 'templates' array
[WARN] [NpcDataRepository] Spawn file contains empty 'spawns' array
```

### Test 3: Malformed Data

**Setup:**
- Template with `level: -5` (invalid)

**Expected:**
```
[WARN] [NpcDataRepository] NPC template 1001 has invalid level -5, using 1
```

## Integration Test

### Full Server Startup

1. Start LoginServer
2. Start WorldServer
3. Start ZoneServer for zone 10
4. Connect TestClient
5. Verify zone entry works
6. Check no errors related to NPC loading

**Success Criteria:**
- All servers start without errors
- Client connects successfully
- NPC data loaded and logged
- No runtime errors or crashes

## Grep Commands for Log Analysis

```bash
# Check NPC loading
grep "NpcDataRepository" zone_server.log

# Check for errors
grep "ERROR" zone_server.log

# Check for warnings
grep "WARN" zone_server.log

# Check template loading
grep "Loaded NPC template" zone_server.log

# Check spawn loading
grep "Loaded spawn" zone_server.log

# Check summary
grep "Loaded.*template(s)" zone_server.log
grep "Loaded.*spawn point(s)" zone_server.log
```

## Next Steps After Verification

1. **Create More Test Data**
   - Multiple zones with different NPC sets
   - Test zones with no NPCs
   - Test zones with 100+ spawns

2. **Implement Spawn Instantiation**
   - Create runtime ZoneNpc instances from templates
   - Position NPCs at spawn points
   - Track spawn-to-NPC mapping

3. **Add Respawn Logic**
   - Timer-based respawning
   - Respawn variance
   - Weighted group selection

4. **Performance Profiling**
   - Measure load times with large datasets
   - Memory profiling
   - Optimize if needed

## Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| File not found | Wrong path or file missing | Check path is relative to executable |
| Parse error | Invalid JSON syntax | Validate JSON with online tool |
| Duplicate IDs | Copy-paste error | Ensure unique IDs for all entries |
| Invalid references | NPC ID typo | Cross-reference spawn npc_id with template id |
| Empty arrays | Missing data | Add at least one entry to test |

## Documentation References

- JSON Schema: `docs/NPC_DATA_FORMAT.md`
- Architecture: `docs/NPC_SPAWN_SYSTEM_DIAGRAMS.md`
- Code: `REQ_ZoneServer/include/req/zone/NpcSpawnData.h`
