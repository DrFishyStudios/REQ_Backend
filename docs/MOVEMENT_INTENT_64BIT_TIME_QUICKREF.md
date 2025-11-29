# MovementIntent 64-bit ClientTimeMs - Quick Reference

## TL;DR

MovementIntent now uses 64-bit `clientTimeMs` and gracefully handles invalid values instead of rejecting the entire message.

---

## Changes Made

### 1. MovementIntentData Structure
```cpp
// BEFORE
std::uint32_t clientTimeMs{ 0 };  // Max: 4,294,967,295

// AFTER
std::uint64_t clientTimeMs{ 0 };  // Max: 18,446,744,073,709,551,615
```

### 2. Parsing Behavior
```cpp
// BEFORE - Failed entire parse
if (!parseUInt(tokens[6], outData.clientTimeMs)) {
    logError(...);
    return false;  // ? Rejects whole MovementIntent
}

// AFTER - Tolerant with fallback
try {
    outData.clientTimeMs = std::stoull(tokens[6]);
} catch (...) {
    logWarn(...);  // ?? Warning only
    outData.clientTimeMs = 0;  // ? Safe default
}
return true;  // ? Still processes movement
```

---

## Error Handling

### Critical Fields (Must Be Valid)
Movement **REJECTED** if invalid:
- characterId
- sequenceNumber
- inputX / inputY
- facingYawDegrees
- isJumpPressed

### Non-Critical Field (Tolerant)
Movement **ACCEPTED** even if invalid:
- clientTimeMs ? defaults to 0 with warning

---

## Expected Logs

### Before Fix
```
[ERROR] [Protocol] MovementIntent: failed to parse clientTimeMs from '17264544641'
```
**Result:** Movement ignored ?

### After Fix
```
[WARN] [Protocol] MovementIntent: invalid clientTimeMs '17264544641' (...), defaulting to 0
```
**Result:** Movement processed ?

### Normal Operation (No Issues)
```
(No logs - silent success)
```
**Result:** Movement processed ?

---

## Testing

### Test: Unreal Client Movement
1. Start all servers
2. Connect Unreal client
3. Enter zone and move around

**Expected:**
- ? Movement works
- ?? May see warning if timestamp > uint32_t max
- ? No parse errors
- ? No movement rejection

---

## Files Modified

1. ? `REQ_Shared/include/req/shared/ProtocolSchemas.h`
   - Changed `clientTimeMs` to `uint64_t`

2. ? `REQ_Shared/src/ProtocolSchemas.cpp`
   - Made `clientTimeMs` parsing tolerant
   - Uses `std::stoull` for 64-bit support
   - Defaults to 0 on failure

---

## Build Status

? Build successful

---

## Summary

**Problem:** Large clientTimeMs values rejected MovementIntent  
**Solution:** 64-bit type + tolerant parsing with safe default  
**Result:** Movement works regardless of timestamp value! ??
