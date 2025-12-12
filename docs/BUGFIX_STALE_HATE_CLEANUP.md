# Bug Fix: Stale Hate Cleanup (B2-4)

## Problem Description

NPCs were holding hate on players who had died, disconnected, or left the zone, causing "ghost aggro" where NPCs would continue targeting invalid entities.

### Symptoms
- NPCs remain engaged with dead or disconnected players
- `getTopHateTarget()` returns characterIds that no longer exist in `players_` map
- NPCs fail to retarget to valid players even when attacked
- Hate tables accumulate stale entries over long server uptimes
- NPCs may appear "stuck" in combat even though no valid targets remain

---

## Solution

Implemented **Option B** from the threat/hate system audit: a centralized helper function that removes a character from all NPC hate tables, called at appropriate player lifecycle events.

### Implementation Details

#### 1. Helper Method: `removeCharacterFromAllHateTables()`

**Location:** `REQ_ZoneServer/src/ZoneServer_Npc.cpp`

**Purpose:** Remove a specific character from all NPC hate tables and handle AI state transitions

**Algorithm:**
1. Iterate over all NPCs in `npcs_` map
2. For each NPC, check if it has hate for the character
3. If hate entry exists:
   - Erase the entry from `npc.hateTable`
   - Increment `numNpcsTouched` counter
   - If hate table is now empty, increment `numTablesCleared` counter
4. If the character was the NPC's current target:
   - Recompute `npc.currentTargetId` using `getTopHateTarget()`
   - If no new target and NPC is in `Engaged` state, transition to `Leashing` state
5. Log a single summary line with counts

**Code:**
```cpp
void ZoneServer::removeCharacterFromAllHateTables(std::uint64_t characterId) {
    int numNpcsTouched = 0;
    int numTablesCleared = 0;

    for (auto& [npcId, npc] : npcs_) {
        // Check if this NPC has hate for the character
        auto hateIt = npc.hateTable.find(characterId);
        if (hateIt == npc.hateTable.end()) {
            continue; // NPC doesn't have hate for this character
        }

        // Remove hate entry
        npc.hateTable.erase(hateIt);
        numNpcsTouched++;

        // Check if hate table is now empty
        if (npc.hateTable.empty()) {
            numTablesCleared++;
        }

        // If this was the current target, recompute target
        if (npc.currentTargetId == characterId) {
            std::uint64_t newTarget = getTopHateTarget(npc);
            npc.currentTargetId = newTarget;

            // If no new target and NPC is engaged, transition to leashing
            if (newTarget == 0 && npc.aiState == req::shared::data::NpcAiState::Engaged) {
                npc.aiState = req::shared::data::NpcAiState::Leashing;

                req::shared::logInfo("zone", std::string{"[HATE] NPC "} + std::to_string(npc.npcId) +
                    " \"" + npc.name + "\" lost target (character removed), transitioning to Leashing");
            }
        }
    }

    if (numNpcsTouched > 0) {
        req::shared::logInfo("zone", std::string{"[HATE] Removed characterId="} +
            std::to_string(characterId) + " from " + std::to_string(numNpcsTouched) +
            " NPC hate table(s) (" + std::to_string(numTablesCleared) + " cleared)");
    }
}
```

---

#### 2. Call Site: Player Death

**Location:** `REQ_ZoneServer/src/ZoneServer_Death.cpp`

**When:** After marking player as dead but before saving character

**Why:** When a player dies, all NPCs should immediately forget them to allow proper retargeting

**Code:**
```cpp
// Mark player as dead
player.isDead = true;
player.hp = 0;

// Update ZonePlayer state from character
player.level = character->level;
player.xp = character->xp;
player.combatStatsDirty = true;

// Remove from all NPC hate tables (prevents ghost aggro)
req::shared::logInfo("zone", "[DEATH] Removing character from all NPC hate tables");
removeCharacterFromAllHateTables(player.characterId);
```

---

#### 3. Call Site: Player Disconnect / Zone Exit

**Location:** `REQ_ZoneServer/src/ZoneServer_Players.cpp`

**When:** In `removePlayer()`, after saving position but before removing from maps

**Why:** When a player disconnects or leaves the zone, all NPCs should forget them immediately

**Code:**
```cpp
// Remove from all NPC hate tables before removing from zone
req::shared::logInfo("zone", "[REMOVE_PLAYER] Removing from all NPC hate tables");
removeCharacterFromAllHateTables(player.characterId);

// Remove from connection mapping (if connection still exists)
if (player.connection) {
    auto connIt = connectionToCharacterId_.find(player.connection);
    if (connIt != connectionToCharacterId_.end()) {
        connectionToCharacterId_.erase(connIt);
        req::shared::logInfo("zone", "[REMOVE_PLAYER] Removed from connection mapping");
    }
}

// Remove from players map
players_.erase(it);
req::shared::logInfo("zone", "[REMOVE_PLAYER] Removed from players map");
```

---

## Impact on Existing Systems

### Interaction with AI State Machine

**Before Fix:**
- NPCs in `Engaged` state would continue targeting disconnected players
- NPCs would fail to retarget even when attacked by valid players
- NPCs might never leash back because they still had a "target" (invalid)

**After Fix:**
- When current target is removed, `getTopHateTarget()` finds next valid target
- If no valid targets remain, NPC transitions to `Leashing` state automatically
- NPCs return to spawn, heal to full, and become available for new encounters

### Interaction with Hate Accumulation

**Unchanged Behavior:**
- `addHate()` continues to add hate for valid attacks/damage
- Social aggro and assist mechanics continue to work
- Hate decay (if/when implemented) will work independently

**New Behavior:**
- Hate entries are cleaned up proactively at lifecycle events
- No accumulation of stale entries over time
- Hate tables remain accurate and reflect only active players

---

## Performance Considerations

### Complexity
- **Time:** O(N) where N = number of NPCs in zone
- **Space:** No additional memory required
- **Frequency:** Only called on player death or disconnect (rare events)

### Optimization Opportunities
If performance becomes a concern with many NPCs (1000+), consider:
1. Maintain reverse index: `std::unordered_map<characterId, std::vector<npcId>>`
2. Only iterate NPCs in the reverse index for the character
3. Trade-off: More memory, more bookkeeping, but faster cleanup

**Current Decision:** Simple O(N) iteration is sufficient for typical zone sizes (< 500 NPCs)

---

## Expected Log Output

### Example: Player Dies in Combat

```
[INFO] [zone] [DEATH] ========== PLAYER DEATH BEGIN ==========
[INFO] [zone] [DEATH] characterId=5
[INFO] [zone] [DEATH] XP loss applied: characterId=5, level=10 -> 10, xp=5000 -> 4800 (lost 200)
[INFO] [zone] [DEATH] Corpse created: corpseId=1, owner=5, pos=(100.5,200.0,10.0)
[INFO] [zone] [DEATH] Removing character from all NPC hate tables
[INFO] [zone] [HATE] NPC 1001 "A Skeleton" lost target (character removed), transitioning to Leashing
[INFO] [zone] [HATE] NPC 1003 "A Zombie" lost target (character removed), transitioning to Leashing
[INFO] [zone] [HATE] Removed characterId=5 from 3 NPC hate table(s) (2 cleared)
[INFO] [zone] [DEATH] Character saved successfully
[INFO] [zone] [DEATH] ========== PLAYER DEATH END ==========
```

**Key Points:**
- One summary log line for hate cleanup
- Per-NPC logs only for those that transition to Leashing
- Clear indication of how many NPCs were affected

---

### Example: Player Disconnects During Combat

```
[INFO] [zone] [DISCONNECT] ========== BEGIN DISCONNECT HANDLING ==========
[INFO] [zone] [DISCONNECT] Found ZonePlayer: characterId=7
[INFO] [zone] [REMOVE_PLAYER] BEGIN: characterId=7
[INFO] [zone] [REMOVE_PLAYER] Attempting to save character state...
[INFO] [zone] [SAVE] Position saved successfully: characterId=7
[INFO] [zone] [REMOVE_PLAYER] Character state saved successfully
[INFO] [zone] [REMOVE_PLAYER] Removing from all NPC hate tables
[INFO] [zone] [HATE] NPC 1005 "An Orc" lost target (character removed), transitioning to Leashing
[INFO] [zone] [HATE] Removed characterId=7 from 2 NPC hate table(s) (1 cleared)
[INFO] [zone] [REMOVE_PLAYER] Removed from connection mapping
[INFO] [zone] [REMOVE_PLAYER] Removed from players map
[INFO] [zone] [REMOVE_PLAYER] END: characterId=7, remaining_players=1
[INFO] [zone] [DISCONNECT] ========== END DISCONNECT HANDLING ==========
```

**Key Points:**
- Hate cleanup happens before removing from player map
- NPCs automatically leash and reset
- Remaining players can continue fighting without interference

---

### Example: Player Leaves Zone (No Hate)

```
[INFO] [zone] [REMOVE_PLAYER] BEGIN: characterId=42
[INFO] [zone] [REMOVE_PLAYER] Attempting to save character state...
[INFO] [zone] [SAVE] Position saved successfully: characterId=42
[INFO] [zone] [REMOVE_PLAYER] Character state saved successfully
[INFO] [zone] [REMOVE_PLAYER] Removing from all NPC hate tables
[INFO] [zone] [REMOVE_PLAYER] Removed from connection mapping
[INFO] [zone] [REMOVE_PLAYER] Removed from players map
[INFO] [zone] [REMOVE_PLAYER] END: characterId=42, remaining_players=0
```

**Key Points:**
- No hate cleanup log if player had no hate entries
- Keeps logs clean when nothing needs to be done
- Still safe to call even with no hate

---

## Testing Checklist

### Test 1: Player Dies in Combat
1. ? Start servers and enter zone
2. ? Attack NPC to generate hate
3. ? Let NPC kill player
4. ? **Expected:** NPC transitions to Leashing immediately
5. ? **Expected:** Hate cleanup log shows NPCs cleared
6. ? **Expected:** NPC returns to spawn and heals

---

### Test 2: Player Disconnects During Combat
1. ? Start servers and enter zone
2. ? Attack NPC to generate hate
3. ? Kill client process (Task Manager / `kill -9`)
4. ? **Expected:** Server logs show hate cleanup
5. ? **Expected:** NPC transitions to Leashing
6. ? **Expected:** NPC available for new players

---

### Test 3: Multiple Players, One Disconnects
1. ? Connect 2 players to same zone
2. ? Both attack same NPC
3. ? Disconnect one player
4. ? **Expected:** Disconnected player removed from hate table
5. ? **Expected:** NPC retargets remaining player
6. ? **Expected:** Combat continues normally

---

### Test 4: Player Disconnects, No Hate
1. ? Enter zone but don't engage in combat
2. ? Disconnect client
3. ? **Expected:** No hate cleanup log (no NPCs affected)
4. ? **Expected:** Clean disconnect with no errors

---

### Test 5: NPC Switches Target After Death
1. ? Two players attack same NPC
2. ? Player A has highest hate
3. ? Player A dies
4. ? **Expected:** NPC immediately switches to Player B
5. ? **Expected:** Hate cleanup log shows removal
6. ? **Expected:** Combat continues with Player B

---

## Files Modified

### 1. `REQ_ZoneServer/include/req/zone/ZoneServer.h`
**Change:** Added method declaration
```cpp
void removeCharacterFromAllHateTables(std::uint64_t characterId);
```

### 2. `REQ_ZoneServer/src/ZoneServer_Npc.cpp`
**Change:** Implemented `removeCharacterFromAllHateTables()` method
- Iterates all NPCs
- Removes hate entries
- Recomputes targets
- Transitions to Leashing if needed
- Logs summary

### 3. `REQ_ZoneServer/src/ZoneServer_Death.cpp`
**Change:** Call cleanup in `handlePlayerDeath()`
- Added after marking player dead
- Added before saving character

### 4. `REQ_ZoneServer/src/ZoneServer_Players.cpp`
**Change:** Call cleanup in `removePlayer()`
- Added after saving position
- Added before removing from maps

---

## Alignment with Audit Recommendations

### From Audit Report (Section 3):

**Issue:** "When a player disconnects or zones out, their `characterId` remains in NPC hate tables."

**Fix Option B (Selected):**
> "Add explicit `removeHate(npc, entityId)` function called on player disconnect/death"

? **Implemented as `removeCharacterFromAllHateTables()`**

**Why Option B over Option A:**
- **Proactive:** Cleanup happens at known lifecycle events
- **Simple:** No need to validate entries every time `getTopHateTarget()` is called
- **Efficient:** Only runs during rare events (death/disconnect)
- **Clear ownership:** Cleanup logic is centralized in one place

---

## Future Enhancements

### Potential Improvements (Not Implemented Yet)

1. **Reverse Index:**
   - Maintain `std::unordered_map<characterId, std::vector<npcId>>`
   - Only iterate NPCs that actually have hate for the character
   - Trade-off: More memory, faster cleanup

2. **Periodic Validation:**
   - Run validation sweep every N seconds
   - Clean up any entries that slipped through (defense-in-depth)
   - Log warnings if stale entries are found

3. **Hate Decay:**
   - Implement time-based hate decay (separate feature)
   - Would naturally clean up old entries over time
   - Complementary to lifecycle-based cleanup

4. **Admin Debug Command:**
   - `/hate <npcId>` to view hate table contents
   - `/clearaggro <npcId>` to force-clear hate
   - Useful for debugging and GM intervention

---

## Summary

? **Problem:** Stale hate entries for dead/disconnected players  
? **Solution:** Centralized cleanup on death/disconnect  
? **Implementation:** `removeCharacterFromAllHateTables()` method  
? **Call Sites:** `handlePlayerDeath()` and `removePlayer()`  
? **Build Status:** Successful  
? **Performance:** O(N) iteration, acceptable for typical zones  
? **Logging:** Concise summary with per-NPC details only when needed  
? **Testing:** Ready for manual testing  

**The stale hate bug is fixed! NPCs will no longer hold ghost aggro on dead or disconnected players. ??**

---

## Related Documentation

- **Audit Report:** Section 3 (Bugs and Inconsistencies)
- **Hate System Overview:** REQ_GDD_v09.md §7.2-7.3
- **AI State Machine:** ZoneServer_Npc.cpp (`updateNpcAi`)
- **Player Lifecycle:** `docs/ZONE_DISCONNECT_ROBUSTNESS.md`

---

## Verification Commands

### Check Logs for Hate Cleanup
```bash
grep "\[HATE\] Removed characterId" zone_server.log
grep "transitioning to Leashing" zone_server.log
```

### Monitor Disconnect Flow
```bash
grep -A 20 "BEGIN DISCONNECT HANDLING" zone_server.log | grep "\[HATE\]"
```

### Monitor Death Flow
```bash
grep -A 15 "PLAYER DEATH BEGIN" zone_server.log | grep "\[HATE\]"
```

---

**Implementation complete and ready for testing! ?**
