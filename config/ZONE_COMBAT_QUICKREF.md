# Zone Combat State - Quick Reference

## Player Combat State

### ZonePlayer Fields
```cpp
std::int32_t level, hp, maxHp, mana, maxMana;
std::int32_t strength, stamina, agility, dexterity, intelligence, wisdom, charisma;
bool combatStatsDirty;  // Set when HP/mana changes
```

### Initialization (ZoneAuth)
```cpp
player.level = character->level;
player.hp = (character->hp > 0) ? character->hp : character->maxHp;
player.maxHp = character->maxHp;
// ...all stats...
player.combatStatsDirty = false;
```

### Persistence (Disconnect/Autosave)
```cpp
if (player.combatStatsDirty) {
    character->hp = player.hp;
    character->mana = player.mana;
    // ...save all stats...
    characterStore_.saveCharacter(*character);
}
```

---

## NPC Management

### ZoneNpc Struct
```cpp
std::uint64_t npcId;
std::string name;
std::int32_t level, currentHp, maxHp, minDamage, maxDamage;
float posX, posY, posZ, facingDegrees;
float aggroRadius, leashRadius;
bool isAlive;
```

### NPC JSON Format
```json
{
  "npc_id": 1001,
  "name": "A Skeleton",
  "level": 1,
  "max_hp": 20,
  "min_damage": 1,
  "max_damage": 3,
  "pos_x": 100.0,
  "pos_y": 50.0,
  "pos_z": 0.0
}
```

### Loading NPCs
```cpp
// In ZoneServer::run()
loadNpcsForZone();  // Reads config/zones/zone_<zoneId>_npcs.json

// Access NPCs
std::unordered_map<uint64_t, ZoneNpc> npcs_;
auto& npc = npcs_[1001];  // Get NPC by ID
```

---

## Combat Flow (Next Step)

### AttackRequest Handler
```cpp
case MessageType::AttackRequest: {
    // 1. Parse request
    AttackRequestData req;
    parseAttackRequestPayload(body, req);
    
    // 2. Find attacker (player)
    auto& player = players_[req.attackerCharacterId];
    
    // 3. Find target (NPC or player)
    auto& npc = npcs_[req.targetId];
    
    // 4. Validate range
    float dist = distance(player.pos, npc.pos);
    if (dist > 5.0f) {
        sendAttackResult(..., resultCode=1, "Out of range");
        return;
    }
    
    // 5. Calculate damage
    int dmg = player.level + player.strength/10 + rand();
    
    // 6. Apply damage
    npc.currentHp -= dmg;
    if (npc.currentHp <= 0) {
        npc.isAlive = false;
    }
    
    // 7. Send result
    sendAttackResult(..., damage=dmg, remainingHp=npc.currentHp);
    break;
}
```

---

## Logging Grep Patterns

```bash
# Combat state init
grep "\[COMBAT\]" zone_server.log

# NPC loading
grep "\[NPC\]" zone_server.log

# Combat saves
grep "\[SAVE\] Combat stats" zone_server.log

# All combat-related
grep -E "\[COMBAT\]|\[NPC\]" zone_server.log
```

---

## File Locations

- **Player combat:** `REQ_ZoneServer/include/req/zone/ZoneServer.h` (ZonePlayer)
- **NPC struct:** `REQ_Shared/include/req/shared/DataModels.h` (ZoneNpc)
- **NPC loading:** `REQ_ZoneServer/src/ZoneServer.cpp` (loadNpcsForZone)
- **NPC data:** `config/zones/zone_<zoneId>_npcs.json`
- **Character data:** `data/characters/<characterId>.json`

---

## Common Tasks

### Add New NPC to Zone 10
1. Edit `config/zones/zone_10_npcs.json`
2. Add new entry with unique `npc_id`
3. Restart ZoneServer
4. Check logs for `[NPC] Loaded: id=...`

### Check Player HP/Mana
1. Find character file: `data/characters/<id>.json`
2. Look for `hp`, `maxHp`, `mana`, `maxMana` fields
3. These are saved on disconnect and autosave

### Debug Combat State
1. Enable verbose logging in ZoneServer
2. Watch `[COMBAT]` tags on player entry
3. Watch `[SAVE]` tags on player exit
4. Verify `combatStatsDirty` flag behavior

---

## Next Implementation: Attack Handler

**Pseudocode:**
```cpp
// In handleMessage()
case MessageType::AttackRequest: {
    AttackRequestData req;
    parseAttackRequestPayload(body, req);
    
    // Validate attacker owns connection
    // Find target NPC
    // Check range
    // Calculate damage
    // Apply damage
    // Build AttackResultData
    // Send to attacker (and optionally broadcast)
}
```

**See:** `config/COMBAT_PROTOCOL.md` for message format details
