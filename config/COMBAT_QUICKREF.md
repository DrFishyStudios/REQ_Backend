# Combat System - Quick Reference

## Quick Start

```bash
# 1. Start servers
.\REQ_LoginServer.exe
.\REQ_WorldServer.exe  
.\REQ_ZoneServer.exe --world_id=1 --zone_id=10 --port=7780 --zone_name="East Freeport"

# 2. Run TestClient
.\REQ_TestClient.exe
# Login with defaults (testuser/testpass)

# 3. Attack NPCs
attack 1001    # Skeleton (20 HP)
attack 1002    # Rat (10 HP)
attack 1003    # Beetle (30 HP)
attack 1004    # Snake (15 HP)
attack 1005    # Zombie (50 HP)
```

---

## Commands

| Command | Description |
|---------|-------------|
| `attack <npcId>` | Attack the specified NPC |
| `w` | Move forward |
| `s` | Move backward |
| `a` | Strafe left |
| `d` | Strafe right |
| `j` | Jump |
| `q` | Quit |

---

## NPCs (Zone 10)

| ID | Name | Level | HP | Damage | Location |
|----|------|-------|----|----|----------|
| 1001 | A Decaying Skeleton | 1 | 20 | 1-3 | (100, 50, 0) |
| 1002 | A Rat | 1 | 10 | 1-2 | (200, 75, 0) |
| 1003 | A Fire Beetle | 2 | 30 | 2-5 | (150, 120, 0) |
| 1004 | A Snake | 1 | 15 | 1-4 | (50, 200, 0) |
| 1005 | A Zombie | 3 | 50 | 3-7 | (300, 150, 0) |

---

## Combat Mechanics

### Damage Formula
```
baseDamage = 5 + (level * 2)
strengthBonus = strength / 10
variance = random(-2, +5)
totalDamage = baseDamage + strengthBonus + variance
minimum = 1 damage
```

### Level 1 Player (STR 75)
- Base: 5 + 2 = 7
- Str bonus: 75/10 = 7
- Total: 14-19 damage per hit

### Range & Hit
- Max range: 200 units
- Hit chance: 95%
- Miss chance: 5%

---

## Result Codes

| Code | Meaning |
|------|---------|
| 0 | Success (hit or miss) |
| 1 | Out of range / Invalid target |
| 2 | Not your character |
| 5 | Target is dead |

---

## Debugging

### Check Combat Logs
```bash
# ZoneServer
grep "\[COMBAT\]" zone_server.log

# NPCs
grep "\[NPC\]" zone_server.log

# Deaths
grep "NPC slain" zone_server.log
```

### Expected Output
```
# Attack hit
[COMBAT] AttackRequest: attackerCharId=1, targetId=1001, ...
[COMBAT] Attack hit: attacker=1, target=1001, damage=17, remainingHp=3
[COMBAT] AttackResult: ... dmg=17, hit=1, msg="You hit A Decaying Skeleton for 17..."
[COMBAT] Broadcast AttackResult to 1 client(s)

# Client sees
[CLIENT] AttackResult: ... dmg=17, hit=YES, remainingHp=3, resultCode=0, msg="..."
```

---

## Common Issues

### "Invalid target"
- NPC ID doesn't exist
- Check `zone_10_npcs.json` for valid IDs

### "Target out of range"
- You're too far (>200 units)
- Move closer with w/a/s/d

### "Target is dead"
- NPC already killed
- Attack a different NPC
- Restart ZoneServer to respawn all NPCs

### Attack does nothing
- Check ZoneServer logs for errors
- Verify NPC loaded at startup
- Make sure you're in the zone (zone auth succeeded)

---

## Files

- **Combat logic:** `REQ_ZoneServer/src/ZoneServer.cpp`
- **Attack command:** `REQ_TestClient/src/TestClient_Movement.cpp`
- **NPC data:** `config/zones/zone_10_npcs.json`
- **Protocol:** `REQ_Shared/include/req/shared/ProtocolSchemas.h`

---

## Next Steps

1. **Add respawn:** NPCs respawn after 5 minutes
2. **Add counter-attack:** NPCs fight back
3. **Add aggro:** NPCs attack nearby players
4. **Add loot:** NPCs drop items/gold
5. **Add XP:** Players gain XP for kills

---

## Example Session

```
Movement command: attack 1001
[CLIENT] AttackResult: dmg=15, hit=YES, remainingHp=5, msg="You hit A Decaying Skeleton for 15 points of damage"

Movement command: attack 1001
[CLIENT] AttackResult: dmg=18, hit=YES, remainingHp=0, msg="You hit A Decaying Skeleton for 18 points of damage. A Decaying Skeleton has been slain!"

Movement command: attack 1002
[CLIENT] AttackResult: dmg=12, hit=YES, remainingHp=0, msg="You hit A Rat for 12 points of damage. A Rat has been slain!"

Movement command: attack 1003
[CLIENT] AttackResult: dmg=0, hit=NO, remainingHp=30, msg="You miss A Fire Beetle"

Movement command: attack 1003
[CLIENT] AttackResult: dmg=16, hit=YES, remainingHp=14, msg="You hit A Fire Beetle for 16 points of damage"
```

Happy hunting! ???
