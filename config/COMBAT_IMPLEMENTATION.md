# Simple Combat Implementation - Complete ?

## Summary

Basic combat is now fully functional! Players can attack NPCs using the TestClient, and the ZoneServer processes attacks, calculates damage, updates NPC HP, and broadcasts results to all clients.

---

## Components Implemented

### A) ? ZoneServer: AttackRequest Handler

**Location:** `REQ_ZoneServer/src/ZoneServer.cpp` - `handleMessage()` switch

**Validation Flow:**
1. **Parse Payload:** Parse `attackerCharacterId|targetId|abilityId|isBasicAttack`
2. **Validate Attacker:** Check attacker is valid ZonePlayer
3. **Validate Ownership:** Check connection owns the attacker character
4. **Validate Target:** Check target NPC exists
5. **Process Attack:** Call `processAttack()` for range/damage calculation

**Error Responses:**
- `resultCode=2`: Invalid attacker / Not your character
- `resultCode=1`: Invalid target / Out of range
- `resultCode=5`: Target is already dead

**Logging:**
```
[INFO] [zone] [COMBAT] AttackRequest: attackerCharId=1, targetId=1001, abilityId=0, basicAttack=1
```

### B) ? Damage Calculation & NPC Hit Logic

**Location:** `REQ_ZoneServer/src/ZoneServer.cpp` - `processAttack()`

**Combat Formula:**

1. **Range Check:**
   - Max range: 200 units (Euclidean distance)
   - Formula: `sqrt(dx² + dy² + dz²)`
   - Out of range ? resultCode=1, damage=0

2. **Hit Calculation:**
   - Hit chance: 95%
   - Random roll: 1-100
   - Miss ? resultCode=0, damage=0, wasHit=false

3. **Damage Formula:**
   ```cpp
   baseDamage = 5 + (attacker.level * 2);
   strengthBonus = attacker.strength / 10;
   variance = random(-2, +5);
   totalDamage = baseDamage + strengthBonus + variance;
   totalDamage = max(1, totalDamage); // Minimum 1 damage
   ```

4. **HP Reduction:**
   ```cpp
   target.currentHp -= totalDamage;
   if (target.currentHp <= 0) {
       target.currentHp = 0;
       target.isAlive = false;
       // Log NPC death
   }
   ```

**Example Damage:**
- Level 1 player (STR 75): 5 + 2 + 7 + variance = 14-19 damage
- Level 5 player (STR 85): 5 + 10 + 8 + variance = 23-28 damage

**Logging:**
```
[INFO] [zone] [COMBAT] Attack hit: attacker=1, target=1001, damage=17, remainingHp=3
[INFO] [zone] [COMBAT] NPC slain: npcId=1001, name="A Decaying Skeleton", killerCharId=1
```

### C) ? AttackResult Broadcasting

**Location:** `REQ_ZoneServer/src/ZoneServer.cpp` - `broadcastAttackResult()`

**Broadcast Behavior:**
- Sends to **all clients in the zone** (not just attacker)
- Future: Can optimize to nearby players only

**Result Message Format:**
```
attackerId|targetId|damage|wasHit|remainingHp|resultCode|message
Example: "1|1001|17|1|3|0|You hit A Decaying Skeleton for 17 points of damage"
```

**Result Codes:**
- `0`: Success (hit or miss)
- `1`: Out of range / Invalid target
- `2`: Not your character / Invalid attacker
- `3`: Ability on cooldown (future)
- `4`: Not enough mana (future)
- `5`: Target is dead
- `6`: Line of sight blocked (future)

**Logging:**
```
[INFO] [zone] [COMBAT] AttackResult: attacker=1, target=1001, dmg=17, hit=1, remainingHp=3, resultCode=0, msg="You hit A Decaying Skeleton for 17 points of damage"
[INFO] [zone] [COMBAT] Broadcast AttackResult to 1 client(s)
```

### D) ? NPC Death Handling

**Death Detection:**
```cpp
if (target.currentHp <= 0) {
    target.currentHp = 0;
    target.isAlive = false;
    targetDied = true;
}
```

**Death Logging:**
```
[INFO] [zone] [COMBAT] NPC slain: npcId=1001, name="A Decaying Skeleton", killerCharId=1
```

**Death Message:**
```
"You hit A Decaying Skeleton for 20 points of damage. A Decaying Skeleton has been slain!"
```

**Behavior:**
- NPC remains in `npcs_` map with `currentHp=0`, `isAlive=false`
- Future attacks return resultCode=5 "Target is already dead"
- No respawn yet (can add respawn timer later)
- Can remove dead NPCs immediately if desired

---

## E) ? TestClient: Attack Command

**Location:** `REQ_TestClient/src/TestClient_Movement.cpp` - `runMovementTestLoop()`

**Command Syntax:**
```
attack <npcId>
```

**Examples:**
```
attack 1001    # Attack NPC ID 1001 (A Decaying Skeleton)
attack 1002    # Attack NPC ID 1002 (A Rat)
```

**Implementation:**
```cpp
if (command.find("attack ") == 0) {
    std::string npcIdStr = command.substr(7);
    std::uint64_t npcId = std::stoull(npcIdStr);
    
    // Build AttackRequest
    AttackRequestData attackReq;
    attackReq.attackerCharacterId = localCharacterId;
    attackReq.targetId = npcId;
    attackReq.abilityId = 0;
    attackReq.isBasicAttack = true;
    
    // Send to ZoneServer
    sendMessage(*zoneSocket, MessageType::AttackRequest, payload);
}
```

**Error Handling:**
- Invalid NPC ID input ? Catch exception, show usage
- Valid input ? Send AttackRequest
- Server validates target exists

**Logging:**
```
[INFO] [TestClient] Sent AttackRequest: target=1001
```

### F) ? TestClient: AttackResult Display

**Location:** `REQ_TestClient/src/TestClient_Movement.cpp` - message receive loop

**Display Format:**
```
[CLIENT] AttackResult: attackerId=1, targetId=1001, dmg=17, hit=YES, remainingHp=3, resultCode=0, msg="You hit A Decaying Skeleton for 17 points of damage"
```

**Fields Displayed:**
- `attackerId`: Who attacked
- `targetId`: Target NPC ID
- `dmg`: Damage dealt (0 if miss/error)
- `hit`: YES or NO
- `remainingHp`: Target HP after attack
- `resultCode`: Error code (0 = success)
- `msg`: Human-readable message

**Implementation:**
```cpp
if (header.type == MessageType::AttackResult) {
    AttackResultData result;
    parseAttackResultPayload(msgBody, result);
    
    std::cout << "[CLIENT] AttackResult: "
              << "attackerId=" << result.attackerId
              << ", targetId=" << result.targetId
              << ", dmg=" << result.damage
              << ", hit=" << (result.wasHit ? "YES" : "NO")
              << ", remainingHp=" << result.remainingHp
              << ", resultCode=" << result.resultCode
              << ", msg=\"" << result.message << "\"" << std::endl;
}
```

---

## Manual Test Scenario

### Prerequisites
1. LoginServer running on port 7777
2. WorldServer running on port 7778
3. ZoneServer running (zone 10 - East Freeport) on port 7780
4. NPCs loaded from `config/zones/zone_10_npcs.json`

### Test Procedure

**Step 1: Start Servers**
```bash
cd x64/Debug

# Terminal 1
.\REQ_LoginServer.exe

# Terminal 2
.\REQ_WorldServer.exe

# Terminal 3
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7780 --zone_name="East Freeport"
```

**Step 2: Verify NPC Loading**
Check ZoneServer logs for:
```
[INFO] [zone] [NPC] Loading NPCs from: config/zones/zone_10_npcs.json
[INFO] [zone] [NPC] Loaded: id=1001, name="A Decaying Skeleton", level=1, maxHp=20, pos=(100,50,0)
[INFO] [zone] [NPC] Loaded: id=1002, name="A Rat", level=1, maxHp=10, pos=(200,75,0)
[INFO] [zone] [NPC] Loaded: id=1003, name="A Fire Beetle", level=2, maxHp=30, pos=(150,120,0)
[INFO] [zone] [NPC] Loaded: id=1004, name="A Snake", level=1, maxHp=15, pos=(50,200,0)
[INFO] [zone] [NPC] Loaded: id=1005, name="A Zombie", level=3, maxHp=50, pos=(300,150,0)
[INFO] [zone] [NPC] Loaded 5 NPC(s) for zone 10
```

**Step 3: Connect TestClient**
```bash
# Terminal 4
.\REQ_TestClient.exe

# Login prompts
Enter username (default: testuser): [press Enter]
Enter password (default: testpass): [press Enter]
Mode [login/register] (default: login): [press Enter]
```

**Step 4: Enter Zone**
Wait for:
```
=== Zone Auth Completed Successfully ===
Zone auth successful. Movement test starting.

=== Movement Test Commands ===
  w - Move forward
  s - Move backward
  a - Strafe left
  d - Strafe right
  j - Jump
  attack <npcId> - Attack an NPC
  [empty] - Stop moving
  q - Quit movement test
==============================

Movement command:
```

**Step 5: Attack NPCs**
```
Movement command: attack 1001

# Expected ZoneServer logs:
[INFO] [zone] [COMBAT] AttackRequest: attackerCharId=1, targetId=1001, abilityId=0, basicAttack=1
[INFO] [zone] [COMBAT] Attack hit: attacker=1, target=1001, damage=15, remainingHp=5
[INFO] [zone] [COMBAT] AttackResult: attacker=1, target=1001, dmg=15, hit=1, remainingHp=5, resultCode=0, msg="You hit A Decaying Skeleton for 15 points of damage"
[INFO] [zone] [COMBAT] Broadcast AttackResult to 1 client(s)

# Expected TestClient output:
[CLIENT] AttackResult: attackerId=1, targetId=1001, dmg=15, hit=YES, remainingHp=5, resultCode=0, msg="You hit A Decaying Skeleton for 15 points of damage"
```

**Step 6: Kill an NPC**
Attack same NPC again:
```
Movement command: attack 1001

# Expected (second hit, NPC dies):
[INFO] [zone] [COMBAT] Attack hit: attacker=1, target=1001, damage=18, remainingHp=0
[INFO] [zone] [COMBAT] NPC slain: npcId=1001, name="A Decaying Skeleton", killerCharId=1
[CLIENT] AttackResult: attackerId=1, targetId=1001, dmg=18, hit=YES, remainingHp=0, resultCode=0, msg="You hit A Decaying Skeleton for 18 points of damage. A Decaying Skeleton has been slain!"
```

**Step 7: Attack Dead NPC**
```
Movement command: attack 1001

# Expected:
[CLIENT] AttackResult: attackerId=1, targetId=1001, dmg=0, hit=NO, remainingHp=0, resultCode=5, msg="A Decaying Skeleton is already dead"
```

**Step 8: Test Out of Range**
Attack NPC that's far away (e.g., 1005 at pos 300,150):
```
Movement command: attack 1005

# If player is at spawn (0,0,0), distance = ~335 units > 200 max range
# Expected:
[WARN] [zone] [COMBAT] Out of range: distance=334.664, max=200
[CLIENT] AttackResult: attackerId=1, targetId=1005, dmg=0, hit=NO, remainingHp=50, resultCode=1, msg="Target out of range"
```

**Step 9: Move Closer & Attack**
```
Movement command: w
# Press Enter a few times to move forward

Movement command: attack 1002
# Rat is at (200, 75, 0) - reachable from most positions

# Expected:
[CLIENT] AttackResult: attackerId=1, targetId=1002, dmg=12, hit=YES, remainingHp=0, resultCode=0, msg="You hit A Rat for 12 points of damage. A Rat has been slain!"
```

**Step 10: Test Miss**
Keep attacking until you see a miss (5% chance):
```
Movement command: attack 1003

# On a miss:
[INFO] [zone] [COMBAT] Attack missed: attacker=1, target=1003
[CLIENT] AttackResult: attackerId=1, targetId=1003, dmg=0, hit=NO, remainingHp=30, resultCode=0, msg="You miss A Fire Beetle"
```

---

## Expected Log Patterns

### Successful Attack (Hit)
```
[zone] [COMBAT] AttackRequest: attackerCharId=1, targetId=1001, abilityId=0, basicAttack=1
[zone] [COMBAT] Attack hit: attacker=1, target=1001, damage=17, remainingHp=3
[zone] [COMBAT] AttackResult: attacker=1, target=1001, dmg=17, hit=1, remainingHp=3, resultCode=0, msg="..."
[zone] [COMBAT] Broadcast AttackResult to 1 client(s)
```

### Attack Miss
```
[zone] [COMBAT] AttackRequest: ...
[zone] [COMBAT] Attack missed: attacker=1, target=1001
[zone] [COMBAT] AttackResult: ... dmg=0, hit=0, resultCode=0, msg="You miss ..."
```

### NPC Death
```
[zone] [COMBAT] Attack hit: ... damage=20, remainingHp=0
[zone] [COMBAT] NPC slain: npcId=1001, name="A Decaying Skeleton", killerCharId=1
[zone] [COMBAT] AttackResult: ... remainingHp=0, msg="...has been slain!"
```

### Invalid Target
```
[zone] [COMBAT] AttackRequest: ... targetId=9999 ...
[WARN] [zone] [COMBAT] Invalid target: npcId=9999 not found
[zone] [COMBAT] AttackResult: ... dmg=0, hit=0, remainingHp=0, resultCode=1, msg="Invalid target"
```

### Out of Range
```
[zone] [COMBAT] AttackRequest: ...
[WARN] [zone] [COMBAT] Out of range: distance=334.664, max=200
[zone] [COMBAT] AttackResult: ... resultCode=1, msg="Target out of range"
```

---

## Combat Statistics (Example)

**Level 1 Character vs. Decaying Skeleton (20 HP):**
- Hits needed: ~2-3 (assuming 95% hit rate, 15-20 damage per hit)
- Time to kill: ~6-10 seconds (attack command + response delay)
- Miss rate: 5% (1 in 20 attacks)

**Level 1 Character vs. Fire Beetle (30 HP):**
- Hits needed: ~3-4
- Time to kill: ~10-15 seconds

**Level 1 Character vs. Zombie (50 HP):**
- Hits needed: ~5-6
- Time to kill: ~20-30 seconds

---

## Future Enhancements

### Short Term
1. **Auto-respawn:** Dead NPCs respawn after 5 minutes
2. **NPC counter-attack:** NPCs attack back when aggro'd
3. **Aggro radius:** NPCs automatically attack nearby players
4. **Loot drops:** NPCs drop items/gold on death
5. **XP rewards:** Players gain XP for kills

### Medium Term
6. **Cooldowns:** Abilities have cooldown timers
7. **Mana costs:** Abilities consume mana
8. **Line of sight:** Check for obstacles between attacker and target
9. **Damage types:** Physical/magical, resistances
10. **Status effects:** Poison, stun, slow, etc.

### Long Term
11. **PvP combat:** Players can attack each other (with consent/flags)
12. **Group combat:** Parties share XP and loot
13. **Threat/aggro system:** Tank/healer/DPS roles
14. **Spell effects:** Fireball, heal, buff, debuff animations
15. **Combat log:** Scrolling combat text, damage numbers

---

## Debugging Commands

### Check NPC States
```bash
# In ZoneServer logs, filter for NPCs:
grep "\[NPC\]" zone_server.log

# Check NPC loading:
grep "NPC\] Loaded" zone_server.log

# Check NPC deaths:
grep "NPC slain" zone_server.log
```

### Check Combat Events
```bash
# Filter all combat logs:
grep "\[COMBAT\]" zone_server.log

# Check attack requests:
grep "AttackRequest" zone_server.log

# Check attack results:
grep "AttackResult" zone_server.log

# Check damage dealt:
grep "Attack hit" zone_server.log
```

### Check TestClient Output
```bash
# In TestClient console, look for:
[CLIENT] AttackResult: ...
```

---

## Known Limitations

1. **No NPC respawn:** Dead NPCs stay dead until server restart
2. **No NPC AI:** NPCs don't fight back or move
3. **Simple damage:** No armor, resistances, or damage types
4. **No cooldowns:** Can spam attacks instantly
5. **No mana cost:** All attacks are free
6. **Global broadcast:** All clients receive all AttackResults (not optimized)
7. **No loot:** NPCs don't drop anything
8. **No XP:** Killing NPCs gives no rewards

All of these are easy to add incrementally!

---

## Files Modified

1. **REQ_ZoneServer/include/req/zone/ZoneServer.h**
   - Added `processAttack()` declaration
   - Added `broadcastAttackResult()` declaration
   - Added `#include "ProtocolSchemas.h"` for combat types

2. **REQ_ZoneServer/src/ZoneServer.cpp**
   - Added `AttackRequest` handler in `handleMessage()` switch
   - Implemented `processAttack()` with full combat logic
   - Implemented `broadcastAttackResult()` for zone-wide broadcast
   - Added `#include <random>` for hit/damage RNG

3. **REQ_TestClient/src/TestClient_Movement.cpp**
   - Added "attack <npcId>" command parsing
   - Added `AttackResult` message handler
   - Added `AttackResult` display formatting
   - Updated help text with attack command

---

## Summary Checklist

? **ZoneServer handles AttackRequest** - Validation, error responses  
? **Damage calculation** - Range check, hit/miss, formula, HP reduction  
? **AttackResult broadcasting** - Zone-wide, detailed logging  
? **NPC death detection** - HP=0, isAlive=false, death logging  
? **TestClient attack command** - "attack <npcId>" sends request  
? **TestClient result display** - Formatted AttackResult output  
? **Build successful** - No compilation errors  
? **Manual test ready** - Full scenario documented  

**Combat is live! ??**

---

## Quick Test

```bash
# 1. Start all servers
.\REQ_LoginServer.exe
.\REQ_WorldServer.exe
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7780 --zone_name="East Freeport"

# 2. Run TestClient
.\REQ_TestClient.exe
# Login with defaults

# 3. Attack NPCs
attack 1001    # Attack skeleton
attack 1001    # Kill skeleton
attack 1002    # Attack rat
attack 1002    # Kill rat
attack 1003    # Attack beetle (survives)
attack 1005    # Probably out of range

# 4. Watch combat happen!
```

That's it - you now have working EverQuest-style combat! ???
