# Combat Protocol Implementation - Complete ?

## Summary

A minimal combat protocol has been added to REQ_Shared to support `AttackRequest` and `AttackResult` messages between clients and ZoneServer.

---

## A) ? MessageType Enum Extended

**File:** `REQ_Shared/include/req/shared/MessageTypes.h`

**New Entries:**
```cpp
// Zone gameplay - Combat
AttackRequest         = 42, // Client requests to attack a target
AttackResult          = 43, // ZoneServer sends attack result to client(s)
```

**Positioning:**
- Added immediately after `PlayerStateSnapshot = 41`
- Explicit numeric values assigned (42, 43)
- No renumbering of existing message types
- Maintains backward compatibility

---

## B) ? Data Structures Defined

**File:** `REQ_Shared/include/req/shared/ProtocolSchemas.h`

### AttackRequestData

```cpp
struct AttackRequestData {
    std::uint64_t attackerCharacterId{ 0 };  // Character performing the attack
    std::uint64_t targetId{ 0 };             // Target ID (NPC or player characterId)
    std::uint32_t abilityId{ 0 };            // 0 = basic attack, >0 = specific ability
    bool isBasicAttack{ true };              // Redundant with abilityId=0, but handy for clarity
};
```

---

### AttackResultData

```cpp
struct AttackResultData {
    std::uint64_t attackerId{ 0 };           // Character who performed the attack
    std::uint64_t targetId{ 0 };             // Target that was attacked
    std::int32_t damage{ 0 };                // Damage dealt (0 if miss/dodge)
    bool wasHit{ false };                    // true = hit, false = miss/dodge/parry
    std::int32_t remainingHp{ 0 };           // Target HP after damage (0 = dead)
    std::int32_t resultCode{ 0 };            // 0 = OK, non-zero = error
    std::string message;                     // Human-readable summary
};
```

**Result Codes:**
- `0`: Success
- `1`: Out of range
- `2`: Invalid target
- `3`: Ability on cooldown
- `4`: Not enough mana/energy
- `5`: Target is dead
- `6`: Line of sight blocked

---

## C) ? Build/Parse Helpers Implemented

**File:** `REQ_Shared/src/ProtocolSchemas.cpp`

### AttackRequest Payload Format

**Wire Format:** `attackerCharacterId|targetId|abilityId|isBasicAttack`

**Examples:**
- `"42|1001|0|1"` ? Character 42 basic-attacks NPC 1001
- `"42|43|5|0"` ? Character 42 uses ability 5 on player 43

---

### AttackResult Payload Format

**Wire Format:** `attackerId|targetId|damage|wasHit|remainingHp|resultCode|message`

**Examples:**
- `"42|1001|25|1|75|0|Hit for 25 damage"` ? Success, 25 damage, target has 75 HP
- `"42|1001|0|0|100|1|Target out of range"` ? Miss, out of range

---

## D) ? Testing Complete

**Build Status:** ? Successful

**Manual Verification:**
- Build helpers produce correct wire format
- Parse helpers extract all fields correctly
- Boolean conversions work (0/1)
- Error handling for malformed payloads
- Consistent with existing protocol

---

## Summary

? **A) MessageType enum extended** - Added `AttackRequest = 42` and `AttackResult = 43`  
? **B) Data structures defined** - `AttackRequestData` and `AttackResultData`  
? **C) Build/parse helpers** - Implemented for both message types  
? **D) Testing complete** - Build successful, ready for integration  

**The combat protocol foundation is ready for ZoneServer/TestClient implementation! ??**
