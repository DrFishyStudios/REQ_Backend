# Hardened Handshake Error Handling Guide

## Overview

All handshake protocols (Login, WorldAuth, CharacterList, EnterWorld, ZoneAuth) implement **guaranteed response** semantics:
- Every request receives either an OK or ERR response
- No silent failures or dropped connections
- All error paths are logged with context
- Consistent error code naming across servers

---

## ZoneAuth Handshake (Primary Focus)

### Wire Protocol Specification

**ZoneAuthRequest** (client ? ZoneServer)
```
Wire Format: handoffToken|characterId
Delimiter: | (pipe character)

Field 1: handoffToken (uint64_t as decimal string)
  - Obtained from EnterWorldResponse or WorldAuthResponse
  - Must be non-zero (0 = InvalidHandoffToken)
  - Example: "987654321"

Field 2: characterId (uint64_t as decimal string)
  - Character entering the zone
  - Example: "42"

Complete Example: "987654321|42"
```

**ZoneAuthResponse** (ZoneServer ? client)
```
Success Format: OK|welcomeMessage
Error Format: ERR|errorCode|errorMessage

Success Fields:
  1. status: "OK"
  2. welcomeMessage: human-readable zone welcome
     Example: "Welcome to East Freeport (zone 10 on world 1)"

Error Fields:
  1. status: "ERR"
  2. errorCode: machine-readable code
  3. errorMessage: human-readable description

Success Example: "OK|Welcome to East Freeport (zone 10 on world 1)"
Error Example: "ERR|INVALID_HANDOFF|Handoff token not recognized or has expired"
```

### ZoneAuth Error Codes

| Error Code | Meaning | Triggered When | Server Action |
|------------|---------|----------------|---------------|
| `PARSE_ERROR` | Malformed request | Payload doesn't match schema | Log ERROR, send ERR response |
| `INVALID_HANDOFF` | Invalid token | handoffToken is 0 or not recognized | Log WARN, send ERR response |
| `HANDOFF_EXPIRED` | Token expired (future) | Token has been used or timed out | Log WARN, send ERR response |
| `WRONG_ZONE` | Wrong zone (future) | Token issued for different zoneId | Log WARN, send ERR response |

### ZoneServer Implementation

**Request Handler Flow:**
```cpp
case MessageType::ZoneAuthRequest:
    // 1. Log request receipt
    logInfo("[ZONEAUTH] Received ZoneAuthRequest, payloadSize=...");
    logInfo("[ZONEAUTH] Raw payload: '...'");
    
    // 2. Parse payload
    if (!parseZoneAuthRequestPayload(body, handoffToken, characterId)) {
        logError("[ZONEAUTH] PARSE FAILED - sending error response");
        sendErrorResponse("PARSE_ERROR", "Malformed zone auth request - expected format: handoffToken|characterId");
        return;
    }
    
    // 3. Log parsed fields
    logInfo("[ZONEAUTH] Parsed successfully:");
    logInfo("[ZONEAUTH]   handoffToken=...");
    logInfo("[ZONEAUTH]   characterId=...");
    
    // 4. Validate handoffToken
    if (handoffToken == InvalidHandoffToken) {
        logWarn("[ZONEAUTH] INVALID handoff token (value=0) - sending error response");
        sendErrorResponse("INVALID_HANDOFF", "Handoff token not recognized or has expired");
        return;
    }
    
    // 5. TODO: Real validation (integrate with session service)
    logInfo("[ZONEAUTH] Validating handoff token (using stub validation - TODO: integrate with session service)");
    logInfo("[ZONEAUTH] Handoff token validation PASSED (stub)");
    
    // 6. Create ZonePlayer
    logInfo("[ZONEAUTH] Creating ZonePlayer entry for characterId=...");
    // ... create player ...
    logInfo("[ZONEAUTH] ZonePlayer initialized: characterId=..., spawn=(x,y,z)");
    
    // 7. Send success response
    logInfo("[ZONEAUTH] Sending SUCCESS response:");
    logInfo("[ZONEAUTH]   type=31, payloadSize=..., payload='OK|...'");
    sendSuccessResponse(welcomeMessage);
    
    // 8. Log completion
    logInfo("[ZONEAUTH] COMPLETE: characterId=... successfully entered zone \"...\"");
```

**Guaranteed Response:**
- ? Parse failure ? ERR response with PARSE_ERROR
- ? Invalid token ? ERR response with INVALID_HANDOFF
- ? Validation success ? OK response with welcome message
- ? All paths logged with context

---

## WorldAuth Handshake

### Wire Protocol Specification

**WorldAuthRequest** (client ? WorldServer)
```
Wire Format: sessionToken|worldId
Delimiter: | (pipe character)

Field 1: sessionToken (uint64_t as decimal string)
  - From LoginResponse
  - Example: "123456789"

Field 2: worldId (uint64_t as decimal string)
  - World to authenticate into
  - Example: "1"

Complete Example: "123456789|1"
```

**WorldAuthResponse** (WorldServer ? client)
```
Success Format: OK|handoffToken|zoneId|zoneHost|zonePort
Error Format: ERR|errorCode|errorMessage

Success Fields:
  1. status: "OK"
  2. handoffToken: decimal handoff token for zone server
  3. zoneId: decimal zone ID to connect to
  4. zoneHost: zone server hostname/IP
  5. zonePort: zone server port (decimal)

Error Fields:
  1. status: "ERR"
  2. errorCode: machine-readable code
  3. errorMessage: human-readable description

Success Example: "OK|987654321|100|127.0.0.1|7779"
Error Example: "ERR|INVALID_SESSION|Session token not recognized"
```

### WorldAuth Error Codes

| Error Code | Meaning | Triggered When |
|------------|---------|----------------|
| `PARSE_ERROR` | Malformed request | Payload doesn't match schema |
| `WRONG_WORLD` | WorldId mismatch | Requested worldId doesn't match server |
| `NO_ZONES` | No zones available | World has no configured zones |
| `INVALID_SESSION` | Session not found | SessionToken not recognized |

### WorldServer Implementation

**Guaranteed Response:**
- ? Parse failure ? ERR with PARSE_ERROR
- ? WorldId mismatch ? ERR with WRONG_WORLD
- ? No zones ? ERR with NO_ZONES
- ? Invalid session ? ERR with INVALID_SESSION
- ? Success ? OK with handoff token and zone endpoint

---

## EnterWorld Handshake (Preferred Flow)

This is the modern flow used by TestClient and should be used by Unreal client.

### Wire Protocol

**EnterWorldRequest**
```
Wire Format: sessionToken|worldId|characterId
Fields: sessionToken, worldId, characterId (all uint64_t as decimal strings)
Example: "123456789|1|42"
```

**EnterWorldResponse**
```
Success Format: OK|handoffToken|zoneId|zoneHost|zonePort
Error Format: ERR|errorCode|errorMessage
```

### EnterWorld Error Codes

| Error Code | Meaning |
|------------|---------|
| `PARSE_ERROR` | Malformed request |
| `WRONG_WORLD` | WorldId mismatch |
| `INVALID_SESSION` | SessionToken not recognized |
| `CHARACTER_NOT_FOUND` | Character doesn't exist |
| `ACCESS_DENIED` | Character belongs to different account |
| `WRONG_WORLD_CHARACTER` | Character's homeWorldId/lastWorldId doesn't match |
| `NO_ZONES` | No zones configured |

---

## Negative Testing

### Running Negative Tests

**Command Line:**
```bash
REQ_TestClient.exe --negative-tests
# or
REQ_TestClient.exe -n
```

**What It Tests:**
1. **Invalid HandoffToken (0):**
   - Sends ZoneAuthRequest with handoffToken=0
   - Expects ERR response with INVALID_HANDOFF
   - Verifies no silent failure

2. **Malformed ZoneAuthRequest:**
   - Sends payload with missing field: "12345" (no |characterId)
   - Expects ERR response with PARSE_ERROR
   - Verifies parse error handling

### Expected Output

**Test 1: Invalid HandoffToken**
```
[INFO] [TestClient] --- Test 1: Invalid HandoffToken (0) ---
[INFO] [TestClient] Sending ZoneAuthRequest with handoffToken=0 (InvalidHandoffToken)
[INFO] [TestClient] Connecting to zone server at 127.0.0.1:7000...
[INFO] [TestClient] Connected
[INFO] [TestClient] Sending: handoffToken=0, characterId=12345
[INFO] [TestClient] Payload: '0|12345'
[INFO] [TestClient] Received ZoneAuthResponse, payload: 'ERR|INVALID_HANDOFF|Handoff token not recognized or has expired'
[INFO] [TestClient] Error response received: errorCode='INVALID_HANDOFF', errorMessage='Handoff token not recognized or has expired'
[INFO] [TestClient] ? Test 1 PASSED: Server correctly rejected invalid handoffToken
```

**ZoneServer Logs:**
```
[WARN] [zone] [ZONEAUTH] INVALID handoff token (value=0) - sending error response
[INFO] [zone] [ZONEAUTH] Sending ERROR response: payload='ERR|INVALID_HANDOFF|Handoff token not recognized or has expired'
```

**Test 2: Malformed Payload**
```
[INFO] [TestClient] --- Test 2: Malformed ZoneAuthRequest payload ---
[INFO] [TestClient] Sending ZoneAuthRequest with malformed payload
[INFO] [TestClient] Sending malformed payload: '12345'
[ERROR] [Protocol] ZoneAuthRequest: expected 2 fields, got 1
[ERROR] [TestClient] Failed to parse ZoneAuthRequest
[INFO] [TestClient] Received ZoneAuthResponse, payload: 'ERR|PARSE_ERROR|Malformed zone auth request - expected format: handoffToken|characterId'
[INFO] [TestClient] ? Test 2 PASSED: Server correctly rejected malformed payload
```

**ZoneServer Logs:**
```
[ERROR] [zone] [ZONEAUTH] PARSE FAILED - sending error response
[INFO] [zone] [ZONEAUTH] Sending ERROR response: type=31, payload='ERR|PARSE_ERROR|Malformed zone auth request - expected format: handoffToken|characterId'
```

---

## Silent Failure Checklist

### ZoneServer
- ? Parse failure ? logs ERROR, sends ERR response
- ? Invalid handoff (0) ? logs WARN, sends ERR response
- ? All error responses logged before sending

### WorldServer (WorldAuth)
- ? Parse failure ? logs ERROR, sends ERR response
- ? WorldId mismatch ? logs WARN, sends ERR response
- ? No zones ? logs ERROR, sends ERR response
- ? Invalid session ? logs WARN, sends ERR response

### WorldServer (EnterWorld)
- ? Parse failure ? logs ERROR, sends ERR response
- ? WorldId mismatch ? logs WARN, sends ERR response
- ? Invalid session ? logs WARN, sends ERR response
- ? Character not found ? logs WARN, sends ERR response
- ? Access denied ? logs WARN, sends ERR response
- ? Wrong world character ? logs WARN, sends ERR response
- ? No zones ? logs ERROR, sends ERR response

---

## Future Enhancements

### ZoneAuth Validation (TODO)
Currently using stub validation that accepts any non-zero handoffToken.

**Planned Real Validation:**
1. **Handoff Token Registry:**
   - WorldServer stores handoffToken ? (characterId, accountId, zoneId, timestamp) in shared store
   - Could be Redis, database, or shared memory

2. **ZoneServer Validation:**
   - Query handoff registry with token
   - Verify token hasn't expired (e.g., 60 second TTL)
   - Verify zoneId matches this ZoneServer
   - Verify characterId matches request
   - Mark token as used (one-time use)

3. **Character Data Loading:**
   - Load character from database
   - Verify character belongs to accountId from token
   - Load saved position, inventory, etc.

4. **Zone Capacity Checking:**
   - Check if zone is full
   - Return ZONE_FULL error if at capacity

### Error Code Additions

**Future ZoneAuth Errors:**
- `HANDOFF_EXPIRED`: Token TTL exceeded
- `HANDOFF_USED`: Token already consumed
- `WRONG_ZONE`: Token issued for different zone
- `ZONE_FULL`: Zone at player capacity
- `CHARACTER_BANNED`: Character is banned from zone
- `WORLD_MISMATCH`: Character's worldId doesn't match

**Future WorldAuth Errors:**
- `WORLD_FULL`: World at player capacity
- `WORLD_OFFLINE`: World in maintenance mode
- `ACCOUNT_SUSPENDED`: Account temporarily suspended

---

## Summary

**? Complete Error Coverage:**
- All parse failures handled
- All validation failures handled
- All responses logged
- No silent failures

**? Consistent Error Format:**
- ERR|errorCode|errorMessage across all servers
- Machine-readable error codes
- Human-readable error messages

**? Testable:**
- Negative test mode in TestClient
- Verifies error responses
- Checks for silent failures

**? Production-Ready:**
- Structured for future session integration
- Clear TODO markers for stub code
- Extensible error code system

With this hardened handshake, any client (TestClient or Unreal) will receive clear, actionable error messages for all failure scenarios, making debugging and client development much easier.
