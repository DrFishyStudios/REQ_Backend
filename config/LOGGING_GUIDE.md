# REQ Backend Standardized Logging Guide

## Overview
All three backend servers (LoginServer, WorldServer, ZoneServer) use a consistent logging format for complete handshake traceability. This enables debugging the entire client journey using only console logs.

## Log Format Standard

```
[YYYY-MM-DD HH:MM:SS] [ExecutableName] [LEVEL] [category] message
```

**Example:**
```
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] LoginRequest: username=Rich, mode=login
```

### Components

| Component | Description | Example |
|-----------|-------------|---------|
| Timestamp | ISO date/time | `2024-01-15 14:32:01` |
| Executable | Server name | `REQ_LoginServer`, `REQ_WorldServer`, `REQ_ZoneServer` |
| Level | Log severity | `INFO`, `WARN`, `ERROR` |
| Category | Functional area | `login`, `world`, `zone`, `Main` |
| Message | Descriptive text | `LoginRequest: username=Rich, mode=login` |

## Logging Categories

| Category | Used By | Purpose |
|----------|---------|---------|
| `Main` | All servers | Startup, shutdown, configuration loading |
| `login` | LoginServer | Authentication, session management |
| `world` | WorldServer | World selection, character management, zone handoffs |
| `zone` | ZoneServer | Zone authentication, player state, movement |
| `Protocol` | All servers (via ProtocolSchemas) | Message parsing errors |
| `TestClient` | TestClient | Client-side operations |

## Complete Handshake Log Trace

### Stage 1: Login/Registration

**TestClient ? LoginServer:**
```
[2024-01-15 14:32:01] [REQ_TestClient] [INFO] [TestClient] Connecting to login server at 127.0.0.1:7777...
[2024-01-15 14:32:01] [REQ_TestClient] [INFO] [TestClient] Connected to login server
[2024-01-15 14:32:01] [REQ_TestClient] [INFO] [TestClient] Sending LoginRequest: username=Rich, clientVersion=REQ-TestClient-0.1, mode=login
```

**LoginServer Processing:**
```
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] New client connected
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] Received message: type=10, protocolVersion=1, payloadSize=45
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] LoginRequest: username=Rich, clientVersion=REQ-TestClient-0.1, mode=login
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] Login successful: username=Rich, accountId=1
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] LoginResponse OK: username=Rich, accountId=1, sessionToken=16874771734187850833, worldCount=2
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login]   World: id=1, name=CazicThule, endpoint=127.0.0.1:7778, ruleset=classic_plus_qol
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login]   World: id=2, name=Karana, endpoint=127.0.0.1:7780, ruleset=classic_hardcore
```

**Error Example (Invalid Password):**
```
[2024-01-15 14:32:01] [REQ_LoginServer] [WARN] [login] Login failed: invalid password for username 'Rich'
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] LoginResponse ERR: errorCode=INVALID_PASSWORD, username=Rich
```

**Registration Example:**
```
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] LoginRequest: username=NewUser, clientVersion=REQ-TestClient-0.1, mode=register
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] Registration successful: username=NewUser, accountId=5
[2024-01-15 14:32:01] [REQ_LoginServer] [INFO] [login] LoginResponse OK: username=NewUser, accountId=5, sessionToken=12345678901234567890, worldCount=2
```

### Stage 2: Character List

**TestClient ? WorldServer:**
```
[2024-01-15 14:32:02] [REQ_TestClient] [INFO] [TestClient] Connecting to world server at 127.0.0.1:7778...
[2024-01-15 14:32:02] [REQ_TestClient] [INFO] [TestClient] Connected to world server
[2024-01-15 14:32:02] [REQ_TestClient] [INFO] [TestClient] Sending CharacterListRequest: sessionToken=16874771734187850833, worldId=1
```

**WorldServer Processing:**
```
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world] New client connected
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world] Received message: type=22, protocolVersion=1, payloadSize=32
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world] CharacterListRequest: sessionToken=16874771734187850833, worldId=1
[2024-01-15 14:32:02] [REQ_WorldServer] [WARN] [world] resolveSessionToken: using stub accountId=1 for testing (TODO: implement shared session service)
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world] CharacterListRequest: accountId=1, worldId=1, characters found=6
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world]   Character: id=1, name=Warrior01, race=Human, class=Warrior, level=5
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world]   Character: id=2, name=Cleric01, race=HighElf, class=Cleric, level=3
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world]   Character: id=3, name=Rogue01, race=DarkElf, class=Rogue, level=7
```

**Error Example (Invalid Session):**
```
[2024-01-15 14:32:02] [REQ_WorldServer] [WARN] [world] Invalid session token
[2024-01-15 14:32:02] [REQ_WorldServer] [INFO] [world] CharacterListResponse ERR: errorCode=INVALID_SESSION
```

### Stage 3: Character Creation (if no characters)

**TestClient ? WorldServer:**
```
[2024-01-15 14:32:03] [REQ_TestClient] [INFO] [TestClient] No characters found. Creating a new character...
[2024-01-15 14:32:03] [REQ_TestClient] [INFO] [TestClient] Sending CharacterCreateRequest: name=TestWarrior, race=Human, class=Warrior
```

**WorldServer Processing:**
```
[2024-01-15 14:32:03] [REQ_WorldServer] [INFO] [world] Received message: type=24, protocolVersion=1, payloadSize=55
[2024-01-15 14:32:03] [REQ_WorldServer] [INFO] [world] CharacterCreateRequest: sessionToken=16874771734187850833, worldId=1, name=TestWarrior, race=Human, class=Warrior
[2024-01-15 14:32:03] [REQ_WorldServer] [WARN] [world] resolveSessionToken: using stub accountId=1 for testing (TODO: implement shared session service)
[2024-01-15 14:32:03] [REQ_WorldServer] [INFO] [world] Character created successfully: id=7, accountId=1, name=TestWarrior, race=Human, class=Warrior
[2024-01-15 14:32:03] [REQ_WorldServer] [INFO] [world] CharacterCreateResponse OK: characterId=7, name=TestWarrior, race=Human, class=Warrior, level=1
```

**Error Example (Name Taken):**
```
[2024-01-15 14:32:03] [REQ_WorldServer] [WARN] [world] Character creation failed: Character name 'TestWarrior' already exists for this account on this world
[2024-01-15 14:32:03] [REQ_WorldServer] [INFO] [world] CharacterCreateResponse ERR: errorCode=NAME_TAKEN, message=Character name already exists
```

### Stage 4: Enter World

**TestClient ? WorldServer:**
```
[2024-01-15 14:32:04] [REQ_TestClient] [INFO] [TestClient] Selecting character: id=1, name=Warrior01
[2024-01-15 14:32:04] [REQ_TestClient] [INFO] [TestClient] Sending EnterWorldRequest: sessionToken=16874771734187850833, worldId=1, characterId=1
```

**WorldServer Processing:**
```
[2024-01-15 14:32:04] [REQ_WorldServer] [INFO] [world] Received message: type=26, protocolVersion=1, payloadSize=40
[2024-01-15 14:32:04] [REQ_WorldServer] [INFO] [world] EnterWorldRequest: sessionToken=16874771734187850833, worldId=1, characterId=1
[2024-01-15 14:32:04] [REQ_WorldServer] [WARN] [world] resolveSessionToken: using stub accountId=1 for testing (TODO: implement shared session service)
[2024-01-15 14:32:04] [REQ_WorldServer] [INFO] [world] EnterWorldResponse OK: characterId=1, characterName=Warrior01, handoffToken=9876543210987654321, zoneId=10, endpoint=127.0.0.1:7000
```

**Error Examples:**
```
# Character not found
[2024-01-15 14:32:04] [REQ_WorldServer] [WARN] [world] Character not found: id=999
[2024-01-15 14:32:04] [REQ_WorldServer] [INFO] [world] EnterWorldResponse ERR: errorCode=CHARACTER_NOT_FOUND

# Character doesn't belong to account
[2024-01-15 14:32:04] [REQ_WorldServer] [WARN] [world] Character ownership mismatch: characterId=5, expected accountId=1, actual accountId=2
[2024-01-15 14:32:04] [REQ_WorldServer] [INFO] [world] EnterWorldResponse ERR: errorCode=ACCESS_DENIED

# Wrong world
[2024-01-15 14:32:04] [REQ_WorldServer] [WARN] [world] Character world mismatch: characterId=5, homeWorldId=2, lastWorldId=2, requested=1
[2024-01-15 14:32:04] [REQ_WorldServer] [INFO] [world] EnterWorldResponse ERR: errorCode=WRONG_WORLD_CHARACTER
```

### Stage 5: Zone Authentication

**TestClient ? ZoneServer:**
```
[2024-01-15 14:32:05] [REQ_TestClient] [INFO] [TestClient] Connecting to zone server at 127.0.0.1:7000...
[2024-01-15 14:32:05] [REQ_TestClient] [INFO] [TestClient] Connected to zone server
[2024-01-15 14:32:05] [REQ_TestClient] [INFO] [TestClient] Sending ZoneAuthRequest: handoffToken=9876543210987654321, characterId=1
```

**ZoneServer Processing:**
```
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] New client connected to zone "East Freeport" (id=10)
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [RECV] Message header: type=30 (enum: 30), protocolVersion=1, payloadSize=25
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Received ZoneAuthRequest, payloadSize=25
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Raw payload: '9876543210987654321|1'
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Parsed successfully:
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH]   handoffToken=9876543210987654321
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH]   characterId=1
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH]   zone="East Freeport" (id=10)
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Validating handoff token (using stub validation - TODO: integrate with session service)
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Handoff token validation PASSED (stub)
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Creating ZonePlayer entry for characterId=1
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] ZonePlayer initialized: characterId=1, spawn=(0,0,0)
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Sending SUCCESS response:
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH]   type=31 (enum: 31)
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH]   payloadSize=78
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH]   payload='OK|Welcome to East Freeport (zone 10 on world 1)'
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] COMPLETE: characterId=1 successfully entered zone "East Freeport"
```

**Error Examples:**
```
# Parse failure
[2024-01-15 14:32:05] [REQ_ZoneServer] [ERROR] [zone] [ZONEAUTH] PARSE FAILED - sending error response
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Sending ERROR response: type=31, payload='ERR|PARSE_ERROR|Malformed zone auth request - expected format: handoffToken|characterId'

# Invalid handoff token
[2024-01-15 14:32:05] [REQ_ZoneServer] [WARN] [zone] [ZONEAUTH] INVALID handoff token (value=0) - sending error response
[2024-01-15 14:32:05] [REQ_ZoneServer] [INFO] [zone] [ZONEAUTH] Sending ERROR response: payload='ERR|INVALID_HANDOFF|Handoff token not recognized or has expired'
```

### Stage 6: Movement (ongoing)

**TestClient ? ZoneServer (input):**
```
[2024-01-15 14:32:10] [REQ_TestClient] [INFO] [TestClient] Sent MovementIntent: seq=1, input=(0,1), jump=0
[2024-01-15 14:32:10] [REQ_TestClient] [INFO] [TestClient] Sent MovementIntent: seq=2, input=(1,0), jump=0
```

**ZoneServer ? All Clients (snapshots):**
```
# These logs are commented out by default to reduce spam at 20Hz
# Can be re-enabled for debugging by uncommenting in ZoneServer.cpp
```

### Player Disconnect

**ZoneServer (planned - not yet implemented):**
```
[2024-01-15 14:35:00] [REQ_ZoneServer] [INFO] [zone] ZonePlayer removed: characterId=1, reason=disconnect
```

## Error Code Reference

### LoginServer Error Codes

| Code | Meaning | Triggered When |
|------|---------|----------------|
| `PARSE_ERROR` | Malformed request | LoginRequest payload doesn't match schema |
| `INVALID_USERNAME` | Empty username | Username field is empty |
| `USERNAME_TAKEN` | Username exists | Registration with existing username |
| `ACCOUNT_NOT_FOUND` | No such account | Login with non-existent username |
| `INVALID_PASSWORD` | Wrong password | Password doesn't match stored hash |
| `ACCOUNT_BANNED` | Account banned | Attempting to login with banned account |

### WorldServer Error Codes

| Code | Meaning | Triggered When |
|------|---------|----------------|
| `PARSE_ERROR` | Malformed request | Request payload doesn't match schema |
| `WRONG_WORLD` | WorldId mismatch | Request worldId doesn't match server worldId |
| `NO_ZONES` | No zones available | World has no configured zones |
| `INVALID_SESSION` | Session not found | SessionToken not recognized |
| `CHARACTER_NOT_FOUND` | Character doesn't exist | CharacterId not in database |
| `ACCESS_DENIED` | Wrong account | Character belongs to different account |
| `WRONG_WORLD_CHARACTER` | Character on different world | Character's homeWorldId/lastWorldId doesn't match |
| `NAME_TAKEN` | Name already used | Character creation with duplicate name |
| `INVALID_RACE` | Invalid race selection | Character creation with unknown race |
| `INVALID_CLASS` | Invalid class selection | Character creation with unknown class |
| `CREATE_FAILED` | Generic creation error | Character creation failed for other reason |

### ZoneServer Error Codes

| Code | Meaning | Triggered When |
|------|---------|----------------|
| `PARSE_ERROR` | Malformed request | ZoneAuthRequest payload doesn't match schema |
| `INVALID_HANDOFF` | Bad handoff token | HandoffToken is 0 or not recognized |
| `HANDOFF_EXPIRED` | Token expired (planned) | Token has timed out or been used |
| `WRONG_ZONE` | Token for different zone (planned) | Token issued for different zoneId |

## Logging Best Practices

### When to Log

**? Always Log:**
- All request/response messages (type, key fields)
- All validation failures with context
- All error responses with error code
- All successful state transitions
- Session/token generation
- Player creation/removal

**?? Consider Logging:**
- Protocol version mismatches (WARN level)
- Fallback/default selections
- Stub implementations (WARN level with TODO)

**? Avoid Logging:**
- Passwords (never log credentials)
- Every tick (20Hz is too noisy)
- Internal implementation details

### Log Levels

**INFO:**
- Normal operations
- Request/response pairs
- State transitions
- Successful validations

**WARN:**
- Validation failures
- Protocol mismatches
- Stub implementations
- Fallback selections
- Rejected requests (still responded to)

**ERROR:**
- Parse failures
- Database errors
- Critical failures
- System errors

### Field Naming Conventions

Use consistent names across all servers:

| Field | Format | Example |
|-------|--------|---------|
| accountId | `accountId=X` | `accountId=1` |
| sessionToken | `sessionToken=X` | `sessionToken=16874771734187850833` |
| worldId | `worldId=X` | `worldId=1` |
| characterId | `characterId=X` | `characterId=42` |
| handoffToken | `handoffToken=X` | `handoffToken=9876543210987654321` |
| zoneId | `zoneId=X` | `zoneId=10` |
| username | `username=X` | `username=Rich` |
| mode | `mode=X` | `mode=login` or `mode=register` |

## Debugging Workflow

### Tracing a Single Client Journey

1. **Grep for username:**
   ```bash
   grep "username=Rich" *.log
   ```

2. **Grep for sessionToken:**
   ```bash
   grep "sessionToken=16874771734187850833" *.log
   ```

3. **Grep for characterId:**
   ```bash
   grep "characterId=1" *.log
   ```

4. **Grep for handoffToken:**
   ```bash
   grep "handoffToken=9876543210987654321" *.log
   ```

### Finding Errors

**All errors across all servers:**
```bash
grep "\[ERROR\]" *.log
```

**All warnings (validation failures):**
```bash
grep "\[WARN\]" *.log
```

**Specific error code:**
```bash
grep "INVALID_PASSWORD" *.log
```

### Live Monitoring

**Watch LoginServer:**
```bash
tail -f REQ_LoginServer.log
```

**Watch WorldServer:**
```bash
tail -f REQ_WorldServer.log
```

**Watch ZoneServer:**
```bash
tail -f REQ_ZoneServer.log
```

**Watch all with color-coded output (Linux/macOS):**
```bash
tail -f REQ_*.log | grep --color=auto "ERROR\|WARN\|"
```

## Silent Failure Checklist

All error paths in handshake now log and respond:

### LoginServer
- ? Parse failure ? logs ERROR, sends ERR response
- ? Empty username ? logs WARN, sends ERR response
- ? Username taken ? logs WARN, sends ERR response
- ? Account not found ? logs WARN, sends ERR response
- ? Invalid password ? logs WARN, sends ERR response
- ? Account banned ? logs WARN, sends ERR response

### WorldServer
- ? Parse failure ? logs ERROR, sends ERR response
- ? WorldId mismatch ? logs WARN, sends ERR response
- ? No zones ? logs ERROR, sends ERR response
- ? Invalid session ? logs WARN, sends ERR response
- ? Character not found ? logs WARN, sends ERR response
- ? Access denied ? logs WARN, sends ERR response
- ? Wrong world character ? logs WARN, sends ERR response
- ? Character creation failure ? logs WARN, sends ERR response

### ZoneServer
- ? Parse failure ? logs ERROR, sends ERR response
- ? Invalid handoff token ? logs WARN, sends ERR response

## Summary

**? Standardized Format:** All servers use identical log format  
**? Complete Traceability:** Every handshake step logged with context  
**? No Silent Failures:** All error paths log and respond  
**? Consistent Field Names:** Same naming across all servers  
**? Debug-Friendly:** Easy to grep for specific tokens/IDs  

With this logging infrastructure, any handshake issue can be diagnosed purely from console logs, without needing to attach a debugger.
