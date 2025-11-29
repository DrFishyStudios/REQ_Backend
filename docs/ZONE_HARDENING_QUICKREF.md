# ZoneServer Hardening - Quick Reference

## TL;DR

ZoneServer now handles malformed MovementIntent messages and unexpected client disconnects without crashing.

---

## Key Changes

### 1. Enhanced MovementIntent Parse Safety

**parseMovementIntentPayload():**
- ? Every field wrapped in try-catch
- ? Detailed error shows which field failed
- ? Full payload logged on error
- ? Never leaves outData undefined

**MovementIntent Handler:**
- ? Rate-limited error logging (summary every 5s)
- ? Never uses intent data after parse failure
- ? All validation checks before using data

### 2. Disconnect Handling (Already Hardened)

**Connection:**
- ? Double-close protection
- ? Disconnect callback called exactly once
- ? Exception protection

**ZoneServer:**
- ? removePlayer() wrapped in try-catch
- ? savePlayerPosition() wrapped in try-catch
- ? Always removes from maps (even if save fails)

---

## Expected Logs

### Parse Error
```
[ERROR] [Protocol] MovementIntent: failed to parse inputX from 'abc': stof: no conversion, payload='42|123|abc|...'
[ERROR] [zone] Failed to parse MovementIntent (errors in last 5s: 1), last payload: '...'
```
**Server continues** ?

### Disconnect
```
[INFO] [zone] [DISCONNECT] BEGIN
[INFO] [zone] [REMOVE_PLAYER] BEGIN: characterId=5
[INFO] [zone] [SAVE] Position saved successfully
[INFO] [zone] [REMOVE_PLAYER] END: remaining_players=0
[INFO] [zone] [DISCONNECT] END
```
**Server continues** ?

---

## Testing

### Test 1: Malformed MovementIntent
- Send corrupted MovementIntent
- **Expected:** Server logs error and continues

### Test 2: Close Unreal Client
- Close window while in zone
- **Expected:** Full disconnect logged, position saved, server continues

### Test 3: Kill Unreal Process
- Kill from Task Manager while moving
- **Expected:** Last position saved, server continues

---

## Safety Guarantees

? Parse never throws uncaught exceptions  
? Failed parse never uses undefined data  
? Rate limiting prevents log spam  
? Disconnects always cleaned up  
? Character save failures don't crash server  
? Player always removed from maps  
? Server continues running  

---

## Files Modified

1. `REQ_Shared/src/ProtocolSchemas.cpp` - Enhanced parse safety
2. `REQ_ZoneServer/src/ZoneServer.cpp` - Rate-limited error logging

---

## Build Status

? Build successful

---

## Summary

**Problem:** Malformed messages and disconnects crashed ZoneServer  
**Solution:** Comprehensive error handling and defensive programming  
**Result:** Production-ready stability! ???
