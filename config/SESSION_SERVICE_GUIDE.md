# SessionService - Shared Session Management

## Overview

SessionService is a shared in-memory session management system for the REQ backend. It replaces stub session validation with a real, thread-safe implementation that all servers (LoginServer, WorldServer, ZoneServer) can use.

---

## Architecture

### Location
- **Header:** `REQ_Shared/include/req/shared/SessionService.h`
- **Implementation:** `REQ_Shared/src/SessionService.cpp`
- **Namespace:** `req::shared`

### Design Pattern
- **Singleton** - Single global instance accessed via `SessionService::instance()`
- **Thread-Safe** - All public methods protected by internal mutex
- **In-Memory** - Sessions stored in `std::unordered_map` (no cross-process sharing)

---

## Data Structures

### SessionRecord

Represents an authenticated session for an account.

```cpp
struct SessionRecord {
    std::uint64_t sessionToken;      // Unique 64-bit random identifier
    std::uint64_t accountId;         // Account that owns this session
    std::chrono::system_clock::time_point createdAt;  // Session creation time
    std::chrono::system_clock::time_point lastSeen;   // Last validation time
    std::int32_t boundWorldId;       // World ID (-1 if unbound)
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `sessionToken` | `uint64_t` | Unique random session identifier |
| `accountId` | `uint64_t` | Account that owns this session |
| `createdAt` | `time_point` | When session was created |
| `lastSeen` | `time_point` | Last validation/use timestamp |
| `boundWorldId` | `int32_t` | World ID this session is bound to (-1 = unbound) |

---

## API Reference

### Instance Access

```cpp
static SessionService& instance();
```

Returns the singleton SessionService instance.

**Thread-Safe:** Yes  
**Example:**
```cpp
auto& sessionService = SessionService::instance();
```

---

### createSession

```cpp
std::uint64_t createSession(std::uint64_t accountId);
```

Creates a new session for an account.

**Parameters:**
- `accountId` - Account ID that owns this session

**Returns:**
- Unique 64-bit session token

**Behavior:**
- Generates random 64-bit token (non-zero, unique)
- Stores SessionRecord with:
  - `createdAt` = now
  - `lastSeen` = now
  - `boundWorldId` = -1 (unbound)
- Logs creation with accountId and token

**Thread-Safe:** Yes

**Example:**
```cpp
auto& sessionService = SessionService::instance();
uint64_t token = sessionService.createSession(accountId);
// token = 16874771734187850833
```

**Logs:**
```
[INFO] [SessionService] Session created: accountId=1, sessionToken=16874771734187850833
```

---

### validateSession

```cpp
std::optional<SessionRecord> validateSession(std::uint64_t sessionToken);
```

Validates a session token and returns session details.

**Parameters:**
- `sessionToken` - Token to validate

**Returns:**
- `SessionRecord` if token is valid
- `std::nullopt` if token not found

**Behavior:**
- Looks up token in session map
- If found:
  - Updates `lastSeen` to now
  - Logs success with accountId and boundWorldId
  - Returns SessionRecord
- If not found:
  - Logs warning
  - Returns nullopt

**Thread-Safe:** Yes

**Example:**
```cpp
auto session = sessionService.validateSession(token);
if (session.has_value()) {
    uint64_t accountId = session->accountId;
    int32_t worldId = session->boundWorldId;
    // ... proceed with handshake ...
} else {
    // Invalid session - send error response
}
```

**Logs (Success):**
```
[INFO] [SessionService] Session validated: sessionToken=16874771734187850833, accountId=1, boundWorldId=-1
```

**Logs (Failure):**
```
[WARN] [SessionService] Session validation failed: sessionToken=99999 not found
```

---

### bindSessionToWorld

```cpp
void bindSessionToWorld(std::uint64_t sessionToken, std::int32_t worldId);
```

Binds a session to a specific world.

**Parameters:**
- `sessionToken` - Session to bind
- `worldId` - World ID to bind to

**Behavior:**
- If session exists:
  - Sets `boundWorldId` to worldId
  - Logs binding with token, worldId, accountId
- If session not found:
  - Logs warning
  - No-op

**Thread-Safe:** Yes

**Example:**
```cpp
sessionService.bindSessionToWorld(token, worldId);
```

**Logs (Success):**
```
[INFO] [SessionService] Session bound to world: sessionToken=16874771734187850833, worldId=1, accountId=1
```

**Logs (Failure):**
```
[WARN] [SessionService] Cannot bind session to world: sessionToken=99999 not found
```

---

### removeSession

```cpp
void removeSession(std::uint64_t sessionToken);
```

Removes a session from the service (logout).

**Parameters:**
- `sessionToken` - Session to remove

**Behavior:**
- If session exists:
  - Removes from map
  - Logs removal with token and accountId
- If session not found:
  - Logs warning
  - No-op

**Thread-Safe:** Yes

**Example:**
```cpp
sessionService.removeSession(token);
```

**Logs (Success):**
```
[INFO] [SessionService] Session removed: sessionToken=16874771734187850833, accountId=1
```

**Logs (Failure):**
```
[WARN] [SessionService] Cannot remove session: sessionToken=99999 not found
```

---

### getSessionCount

```cpp
std::size_t getSessionCount() const;
```

Returns the number of active sessions (for monitoring/debugging).

**Thread-Safe:** Yes

**Example:**
```cpp
size_t count = sessionService.getSessionCount();
// count = 42
```

---

### clearAllSessions

```cpp
void clearAllSessions();
```

Removes all sessions (for testing/shutdown).

**Thread-Safe:** Yes

**Example:**
```cpp
sessionService.clearAllSessions();
```

**Logs:**
```
[INFO] [SessionService] All sessions cleared: count=42
```

---

## JSON Persistence (Optional)

Basic JSON persistence for development/debugging. Not optimized for production.

### loadFromFile

```cpp
bool loadFromFile(const std::string& path);
```

Loads sessions from JSON file, replacing current in-memory sessions.

**Parameters:**
- `path` - Path to JSON file

**Returns:**
- `true` if loaded successfully
- `false` on error

**File Format:**
```json
{
  "sessions": [
    {
      "sessionToken": 16874771734187850833,
      "accountId": 1,
      "createdAt": "2024-01-15T14:32:01",
      "lastSeen": "2024-01-15T14:35:22",
      "boundWorldId": 1
    },
    {
      "sessionToken": 12345678901234567890,
      "accountId": 2,
      "createdAt": "2024-01-15T14:33:15",
      "lastSeen": "2024-01-15T14:33:15",
      "boundWorldId": -1
    }
  ]
}
```

**Thread-Safe:** Yes

**Example:**
```cpp
if (sessionService.loadFromFile("sessions.json")) {
    // Sessions loaded
}
```

**Logs:**
```
[INFO] [SessionService] Sessions loaded from file: path=sessions.json, count=2
```

---

### saveToFile

```cpp
bool saveToFile(const std::string& path) const;
```

Saves current sessions to JSON file.

**Parameters:**
- `path` - Path to JSON file

**Returns:**
- `true` if saved successfully
- `false` on error

**Thread-Safe:** Yes

**Example:**
```cpp
if (sessionService.saveToFile("sessions.json")) {
    // Sessions saved
}
```

**Logs:**
```
[INFO] [SessionService] Sessions saved to file: path=sessions.json, count=2
```

---

## Integration Examples

### LoginServer Integration

Replace stub session token storage with SessionService:

**Before:**
```cpp
// LoginServer.cpp
auto token = generateSessionToken();
sessionTokenToAccountId_[token] = accountId;
```

**After:**
```cpp
// LoginServer.cpp
auto& sessionService = req::shared::SessionService::instance();
auto token = sessionService.createSession(accountId);
```

---

### WorldServer Integration

Replace stub `resolveSessionToken()` with SessionService validation:

**Before:**
```cpp
std::optional<std::uint64_t> WorldServer::resolveSessionToken(req::shared::SessionToken token) const {
    if (token == req::shared::InvalidSessionToken) {
        return std::nullopt;
    }
    
    req::shared::logWarn("world", "resolveSessionToken: using stub accountId=1 for testing (TODO: implement shared session service)");
    return 1; // Stub accountId
}
```

**After:**
```cpp
std::optional<std::uint64_t> WorldServer::resolveSessionToken(req::shared::SessionToken token) const {
    auto& sessionService = req::shared::SessionService::instance();
    
    auto session = sessionService.validateSession(token);
    if (session.has_value()) {
        return session->accountId;
    }
    
    return std::nullopt;
}
```

**Bonus: Bind to World**
```cpp
// After successful world authentication
sessionService.bindSessionToWorld(sessionToken, config_.worldId);
```

---

### ZoneServer Integration (Future)

Handoff tokens can be validated through SessionService:

```cpp
// ZoneServer.cpp - in ZoneAuthRequest handler
auto& sessionService = req::shared::SessionService::instance();

// Validate handoff token (currently stub - future enhancement)
auto session = sessionService.validateSession(handoffToken);
if (!session.has_value()) {
    sendErrorResponse("INVALID_HANDOFF", "Handoff token not recognized");
    return;
}

// Verify character belongs to account
if (session->accountId != expectedAccountId) {
    sendErrorResponse("ACCESS_DENIED", "Character does not belong to this account");
    return;
}
```

---

## Session Token Security

### Generation Algorithm

- **Source:** `std::random_device` seeded `std::mt19937_64`
- **Range:** `1` to `std::numeric_limits<uint64_t>::max()`
- **Uniqueness:** Checked against existing sessions
- **Collision Probability:** Negligible for reasonable session counts

### Security Considerations

**? Sufficient for Emulator:**
- Random 64-bit values
- Collision-resistant
- Fast generation

**? NOT Cryptographically Secure:**
- Not suitable for production financial systems
- Not resistant to timing attacks
- Predictable if RNG state is compromised

**Recommendations for Production:**
- Use cryptographic RNG (e.g., `/dev/urandom`, Windows CryptGenRandom)
- Consider UUIDs or JWTs for cross-service authentication
- Add expiration/timeout mechanism
- Implement rate limiting on session creation

---

## Thread Safety

All public methods are protected by an internal `std::mutex`.

**Safe for:**
- ? Multiple threads calling methods concurrently
- ? LoginServer + WorldServer + ZoneServer all accessing same instance
- ? Concurrent session creation/validation/removal

**Not Safe for:**
- ? Cross-process access (sessions are in-memory only)
- ? Distributed deployments without shared storage

**Production Alternatives:**
- Redis (recommended for multi-server setups)
- Memcached
- Shared database with proper locking

---

## Deployment Considerations

### Single-Process Deployment

**Current Implementation:**
- All servers in same process
- Shared in-memory SessionService
- **Works perfectly** ?

### Multi-Process Deployment

**Current Limitation:**
- Each process has separate SessionService instance
- Sessions not shared across processes
- LoginServer sessions not visible to WorldServer in different process

**Solution:**
- Replace in-memory map with Redis/Memcached
- Keep API the same
- Implement Redis-backed storage in SessionService

**Migration Path:**
```cpp
// 1. Add Redis client to SessionService
// 2. Replace sessions_ map operations with Redis commands
// 3. Keep API identical - no server code changes needed
```

---

## Performance

### Memory Usage

- **Per Session:** ~56 bytes (struct) + 16 bytes (map overhead) = ~72 bytes
- **10,000 sessions:** ~720 KB
- **100,000 sessions:** ~7.2 MB

### Operation Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `createSession` | O(1) average | Hash map insert |
| `validateSession` | O(1) average | Hash map lookup |
| `bindSessionToWorld` | O(1) average | Hash map update |
| `removeSession` | O(1) average | Hash map erase |
| `getSessionCount` | O(1) | Map size |
| `clearAllSessions` | O(n) | Clear all entries |

### Mutex Contention

- Low contention expected for typical workloads
- Consider lock-free alternatives if profiling shows contention
- Redis would move contention to network/Redis server

---

## Logging

All SessionService operations are logged for debugging and auditing.

**Log Categories:**

| Operation | Level | Fields Logged |
|-----------|-------|---------------|
| Create session | INFO | accountId, sessionToken |
| Validate session (success) | INFO | sessionToken, accountId, boundWorldId |
| Validate session (failure) | WARN | sessionToken |
| Bind to world (success) | INFO | sessionToken, worldId, accountId |
| Bind to world (failure) | WARN | sessionToken |
| Remove session (success) | INFO | sessionToken, accountId |
| Remove session (failure) | WARN | sessionToken |
| Clear all | INFO | count |
| Load from file | INFO | path, count |
| Save to file | INFO | path, count |

---

## Testing

### Unit Test Examples

```cpp
// Test session creation
auto& service = SessionService::instance();
uint64_t token = service.createSession(123);
assert(token != 0);

// Test validation
auto session = service.validateSession(token);
assert(session.has_value());
assert(session->accountId == 123);
assert(session->boundWorldId == -1);

// Test binding
service.bindSessionToWorld(token, 1);
session = service.validateSession(token);
assert(session->boundWorldId == 1);

// Test removal
service.removeSession(token);
session = service.validateSession(token);
assert(!session.has_value());
```

### Integration Test Workflow

1. **LoginServer** creates session
2. **WorldServer** validates session
3. **WorldServer** binds session to world
4. **ZoneServer** validates session (future)
5. Logout removes session

---

## Next Steps

### Immediate Integration

1. Update LoginServer to use `createSession()`
2. Update WorldServer to use `validateSession()`
3. Remove stub session code
4. Test full handshake flow

### Future Enhancements

1. **Session Expiration:**
   - Add TTL field to SessionRecord
   - Background thread to clean expired sessions

2. **Redis Backend:**
   - Replace in-memory map with Redis client
   - Enable multi-process deployments

3. **Session Metrics:**
   - Track session creation rate
   - Monitor active session count
   - Alert on abnormal patterns

4. **Handoff Token Validation:**
   - Store handoff tokens in SessionService
   - Validate in ZoneServer
   - One-time use enforcement

---

## Summary

**? Complete Implementation:**
- Thread-safe session management
- Comprehensive logging
- Optional JSON persistence
- Ready for integration

**? Production-Ready Features:**
- Singleton pattern
- Mutex protection
- Error handling
- Detailed logging

**? Easy Integration:**
- Simple API
- Drop-in replacement for stub code
- No changes to server architecture

**?? TODO for Production:**
- Add session expiration/timeout
- Replace in-memory storage with Redis for multi-server
- Add cryptographic token generation
- Implement session cleanup/garbage collection

The SessionService is now ready to replace all stub session code across LoginServer, WorldServer, and ZoneServer.
