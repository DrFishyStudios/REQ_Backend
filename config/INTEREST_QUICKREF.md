# Interest Management - Quick Reference

## TL;DR

ZoneServer snapshots can now use **full broadcast** (all players) or **interest filtering** (nearby players only). Configured per zone via JSON.

---

## Config File

**Location:** `config/zones/zone_<id>_config.json`

**Example (Full Broadcast):**
```json
{
  "zone_id": 10,
  "zone_name": "East Freeport",
  "broadcast_full_state": true,
  "interest_radius": 2000.0
}
```

**Example (Interest Filtering):**
```json
{
  "zone_id": 20,
  "zone_name": "West Commonlands",
  "broadcast_full_state": false,
  "interest_radius": 100.0,
  "debug_interest": true
}
```

---

## Key Fields

| Field | Type | Default | Meaning |
|-------|------|---------|---------|
| `broadcast_full_state` | bool | **true** | **true**: all players, **false**: nearby only |
| `interest_radius` | float | 2000.0 | Max distance for including players (when false) |
| `debug_interest` | bool | false | Enable detailed interest logs |

---

## Modes

### Full Broadcast (Default)

**Config:** `"broadcast_full_state": true`

**Behavior:**
- Each client gets ALL players in snapshot
- Same snapshot sent to all clients
- Current behavior (backward compatible)

**Logs:**
```
[Snapshot] Broadcast snapshot 100 with 5 player(s) to 5 connection(s) [FULL BROADCAST]
```

---

### Interest Filtering

**Config:** `"broadcast_full_state": false`

**Behavior:**
- Each client gets NEARBY players only
- Per-client snapshots (different for each player)
- Players always see themselves

**Logs:**
```
[Snapshot] Broadcast snapshot 100 with INTEREST FILTERING to 5 client(s) [radius=100.0]
```

**Debug Logs (if `debug_interest: true`):**
```
[Snapshot] (filtered) recipientCharId=1, playersIncluded=3 (out of 5 total)
```

---

## Distance Calculation

**2D Distance (XY plane):**
```
distance = sqrt((x2 - x1)² + (y2 - y1)²)
```

**Include if:** `distance <= interest_radius`

---

## Testing

### Test 1: Full Broadcast

1. Create `config/zones/zone_10_config.json`:
   ```json
   {"zone_id": 10, "broadcast_full_state": true}
   ```
2. Start ZoneServer
3. Connect 2 TestClients
4. **Expected:** Both see each other (distance doesn't matter)

---

### Test 2: Interest Filtering - Far Apart

1. Create `config/zones/zone_20_config.json`:
   ```json
   {"zone_id": 20, "broadcast_full_state": false, "interest_radius": 100.0, "debug_interest": true}
   ```
2. Start ZoneServer
3. Connect 2 TestClients
4. Move one to (200, 200, 0) - distance > 100
5. **Expected:** Each client sees only themselves

---

### Test 3: Interest Filtering - Close Together

1. Same config as Test 2
2. Move players within 100 units of each other
3. **Expected:** Both clients see each other

---

## Common Radius Values

| Radius | Use Case |
|--------|----------|
| 50-100 | Small testing zones, close combat |
| 200-500 | Medium zones, typical gameplay |
| 1000-2000 | Large zones, open world |
| 5000+ | Effectively full broadcast |

---

## Logs to Watch

### Startup
```
[zone] Loaded zone config from: config/zones/zone_10_config.json
[zone]   broadcastFullState=false
[zone]   interestRadius=100.0
```

### Runtime (Every ~5 seconds)
```
[zone] [Snapshot] Building snapshot 100 for 5 active player(s)
[zone] [Snapshot] Broadcast snapshot 100 with INTEREST FILTERING to 5 client(s) [radius=100.0]
```

### Debug (Every ~1 second if enabled)
```
[zone] [Snapshot] (filtered) recipientCharId=1, playersIncluded=3 (out of 5 total)
```

---

## Troubleshooting

### Players Don't See Each Other

**Check:**
1. Is `interest_radius` large enough?
2. Are players within radius?
3. Is `debug_interest: true` showing low `playersIncluded`?

**Fix:** Increase radius
```json
{"interest_radius": 5000.0}
```

---

### High CPU Usage

**Cause:** Many players + interest filtering = O(N²) distance checks

**Fix:** 
1. Increase radius (fewer checks per player)
2. Switch to full broadcast
3. Implement spatial indexing (future)

---

## API

### Load Config
```cpp
#include "../../REQ_Shared/include/req/shared/Config.h"

auto config = req::shared::loadZoneConfig("config/zones/zone_10_config.json");
zoneServer.setZoneConfig(config);
```

### Update Config at Runtime
```cpp
ZoneConfig newConfig;
newConfig.broadcastFullState = false;
newConfig.interestRadius = 500.0f;
zoneServer.setZoneConfig(newConfig);
```

---

## Default Behavior

**If no config file:**
- `broadcast_full_state = true` (full broadcast)
- `interest_radius = 2000.0`
- **No breaking changes!**

---

## Performance

| Mode | Snapshots Built | Distance Checks | Bandwidth |
|------|----------------|-----------------|-----------|
| Full Broadcast | 1 per tick | 0 | High |
| Interest Filtering | N per tick | N² per tick | Low |

**Trade-off:** CPU for bandwidth

---

## Quick Command Reference

### Create Zone Config
```bash
mkdir -p config/zones
cat > config/zones/zone_10_config.json << EOF
{
  "zone_id": 10,
  "zone_name": "Test Zone",
  "broadcast_full_state": false,
  "interest_radius": 500.0,
  "debug_interest": true
}
EOF
```

### Start ZoneServer
```bash
cd x64/Debug
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7779 --zone_name="Test Zone"
```

### Watch Logs for Interest Filtering
```bash
# Look for:
# - "broadcastFullState=false" in startup
# - "[INTEREST FILTERING]" in snapshot logs
# - "(filtered)" in debug logs
```

---

## Summary

? **Full Broadcast (default):** All players, simple, current behavior  
? **Interest Filtering:** Nearby only, bandwidth savings  
? **JSON Config:** Per-zone configuration  
? **Debug Logs:** Track player inclusion  
? **Backward Compatible:** No code changes needed  

**Files:**
- Config: `config/zones/zone_<id>_config.json`
- Docs: `config/INTEREST_MANAGEMENT.md`

**Status:** ? **PRODUCTION READY**

Interest management is fully implemented and ready for multi-player testing! ??
