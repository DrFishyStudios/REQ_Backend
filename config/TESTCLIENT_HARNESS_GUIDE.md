# REQ_TestClient - Handshake Test Harness Guide

## Overview

REQ_TestClient is now a comprehensive handshake test harness that provides:
- **Stage-based flow tracking** - Clear visibility into handshake progression
- **Automated scenarios** - Happy path and negative tests
- **Interactive menu** - Easy scenario selection
- **Command-line support** - Scriptable test execution
- **Detailed logging** - Matches server log format for easy correlation

---

## Quick Start

### Interactive Menu Mode

Run without arguments to see the menu:
```bash
REQ_TestClient.exe
```

**Menu Options:**
```
1. Happy Path Scenario (automated full handshake)
2. Bad Password Test
3. Bad Session Token Test
4. Bad Handoff Token Test
5. Negative Tests (malformed payloads)
6. Interactive Mode (original flow)
q. Quit
```

### Command-Line Mode

Run specific scenarios directly:

```bash
# Happy path scenario
REQ_TestClient.exe --happy-path
REQ_TestClient.exe -h

# Negative tests
REQ_TestClient.exe --bad-password
REQ_TestClient.exe --bad-session
REQ_TestClient.exe --bad-handoff
REQ_TestClient.exe --negative-tests

# Interactive original mode
REQ_TestClient.exe --interactive

# Help
REQ_TestClient.exe --help
```

---

## Client Stages

The client tracks its state through the handshake using `EClientStage`:

| Stage | Meaning | Entered When |
|-------|---------|--------------|
| `NotConnected` | Initial state | Client starts |
| `LoginPending` | Awaiting login response | Sent LoginRequest |
| `LoggedIn` | Have sessionToken | Received LoginResponse OK |
| `WorldSelected` | World chosen | Selected world from list |
| `CharactersLoaded` | Have character list | Received CharacterListResponse |
| `EnteringWorld` | Awaiting zone handoff | Sent EnterWorldRequest |
| `InZone` | Ready for gameplay | Received ZoneAuthResponse OK |
| `Error` | Handshake failed | Any error occurred |

### Stage Transition Logging

Every stage transition is logged to both file and console:

```
[CLIENT] Stage: NotConnected ? LoginPending (username=testuser)
[CLIENT] Stage: LoginPending ? LoggedIn (sessionToken=12345, worldId=1)
[CLIENT] Stage: LoggedIn ? WorldSelected (worldId=1, endpoint=127.0.0.1:7778)
[CLIENT] Stage: WorldSelected ? CharactersLoaded (count=6)
[CLIENT] Stage: CharactersLoaded ? EnteringWorld (characterId=2, name=Warrior01)
[CLIENT] Stage: EnteringWorld ? InZone (zoneId=10, characterId=2)
```

---

## Scenario 1: Happy Path

### What It Does

Automated end-to-end handshake test:
1. **Login** - Connects to LoginServer, authenticates
2. **World Selection** - Picks first world from list
3. **Character Handling** - Loads characters or creates default (Human Warrior)
4. **Enter World** - Requests zone handoff
5. **Zone Auth** - Connects to zone, authenticates
6. **Movement Test** - Sends 3 test movement commands

### Usage

**Menu:** Select option `1`  
**Command-Line:** `REQ_TestClient.exe --happy-path`

### Prompts

```
Enter username (default: testuser): [Enter for default]
Enter password (default: testpass): [Enter for default]
```

### Example Output

```
=== Happy Path Scenario ===
This will automatically:
  1. Login to LoginServer
  2. Select first world
  3. Load/create character
  4. Enter world and zone
  5. Send test movement

Enter username (default: testuser): 
Enter password (default: testpass): 

[CLIENT] Stage: NotConnected ? LoginPending (username=testuser)
[INFO] [TestClient] Connecting to login server at 127.0.0.1:7777...
[INFO] [TestClient] Connected to login server
[INFO] [TestClient] Sending LoginRequest: username=testuser, clientVersion=REQ-TestClient-0.2, mode=login

[CLIENT] Stage: LoginPending ? LoggedIn (sessionToken=16874771734187850833, worldId=1)
[CLIENT] Stage: LoggedIn ? WorldSelected (worldId=1, endpoint=127.0.0.1:7778)
[CLIENT] Stage: WorldSelected ? CharactersLoaded (count=6)
[CLIENT] Stage: CharactersLoaded ? EnteringWorld (characterId=1, name=Warrior01)

[INFO] [TestClient] Zone handoff received: handoffToken=9876543210987654321, zoneId=10, endpoint=127.0.0.1:7000

[CLIENT] Stage: EnteringWorld ? InZone (zoneId=10, characterId=1)

Sending 3 test movement commands...
[INFO] [TestClient] Sent MovementIntent seq=1
[INFO] [TestClient] Sent MovementIntent seq=2
[INFO] [TestClient] Sent MovementIntent seq=3

=== HAPPY PATH COMPLETE ===
? Login successful: username=testuser, accountId=1
? World selected: worldId=1
? Character selected: characterId=1, name=Warrior01
? Zone entered: zoneId=10
? Movement test completed

? HAPPY PATH COMPLETE
All stages succeeded:
  Login ? World ? Characters ? EnterWorld ? ZoneAuth ? Movement

Key IDs:
  accountId (stub):  1
  sessionToken:      16874771734187850833
  worldId:           1
  characterId:       1
  handoffToken:      9876543210987654321
  zoneId:            10

Press Enter to exit...
```

### Success Criteria

- ? All stage transitions occur in order
- ? No errors logged
- ? Movement commands accepted by zone server
- ? Final summary shows all key IDs

---

## Scenario 2: Bad Password Test

### What It Does

Tests LoginServer error handling by attempting login with incorrect password.

### Usage

**Menu:** Select option `2`  
**Command-Line:** `REQ_TestClient.exe --bad-password`

### Prompts

```
Username: testuser
Correct password: testpass
Wrong password to test: wrongpassword
```

### Expected Output

```
=== Bad Password Test ===
This test attempts login with incorrect password.

Username: testuser
Correct password: testpass
Wrong password to test: badpassword

[CLIENT] Stage: NotConnected ? LoginPending (username=testuser, password=<wrong>)
[INFO] [TestClient] Connecting to login server at 127.0.0.1:7777...
[WARN] [TestClient] Login failed: INVALID_PASSWORD - Invalid username or password

[CLIENT] Stage: LoginPending ? Error (Login rejected (expected))
[INFO] [TestClient] ? Server correctly rejected bad password

? TEST PASSED: Server correctly rejected bad password
Check server logs for error code (should be INVALID_PASSWORD)

Press Enter to continue...
```

### Success Criteria

- ? LoginServer responds with ERR
- ? Error code is `INVALID_PASSWORD`
- ? Client transitions to Error stage
- ? No crash or silent failure

### Server Logs to Check

```
[WARN] [login] Login failed: invalid password for username 'testuser'
[INFO] [login] LoginResponse ERR: errorCode=INVALID_PASSWORD, username=testuser
```

---

## Scenario 3: Bad Session Token Test

### What It Does

Tests WorldServer error handling by corrupting sessionToken before CharacterListRequest.

### Steps

1. Performs valid login to get real sessionToken
2. Corrupts the token (adds 99999)
3. Sends CharacterListRequest with corrupted token
4. Verifies WorldServer rejects it

### Usage

**Menu:** Select option `3`  
**Command-Line:** `REQ_TestClient.exe --bad-session`

### Expected Output

```
=== Bad Session Token Test ===
This test corrupts the sessionToken before CharacterListRequest.

Username: testuser
Password: testpass

[CLIENT] Stage: NotConnected ? LoginPending (username=testuser)
[INFO] [TestClient] Connected to login server
[CLIENT] Stage: LoginPending ? LoggedIn (sessionToken=16874771734187850833)

[INFO] [TestClient] Valid sessionToken: 16874771734187850833
Valid sessionToken obtained: 16874771734187850833
Corrupted sessionToken:      16874771734287832 (original + 99999)

[CLIENT] Stage: LoggedIn ? WorldSelected (Using corrupted sessionToken)
Sending CharacterListRequest with corrupted token...

[CLIENT] Stage: WorldSelected ? Error (Server rejected corrupted token (expected))
[INFO] [TestClient] ? Server rejected: errorCode='INVALID_SESSION', errorMessage='Session token not recognized'

? TEST PASSED: Server correctly rejected corrupted sessionToken
Error response:
  errorCode:    INVALID_SESSION
  errorMessage: Session token not recognized
Expected errorCode: INVALID_SESSION

Press Enter to continue...
```

### Success Criteria

- ? WorldServer responds with ERR
- ? Error code is `INVALID_SESSION`
- ? Token corruption is logged clearly
- ? No silent failure

### Server Logs to Check

```
[WARN] [world] Invalid session token
[INFO] [world] CharacterListResponse ERR: errorCode=INVALID_SESSION
```

---

## Scenario 4: Bad Handoff Token Test

### What It Does

Tests ZoneServer error handling by corrupting handoffToken before ZoneAuthRequest.

### Steps

1. Performs full handshake: Login ? World ? Characters ? EnterWorld
2. Gets valid handoffToken from EnterWorldResponse
3. Corrupts the token (adds 88888)
4. Sends ZoneAuthRequest with corrupted token
5. Verifies ZoneServer rejects it

### Usage

**Menu:** Select option `4`  
**Command-Line:** `REQ_TestClient.exe --bad-handoff`

### Expected Output

```
=== Bad Handoff Token Test ===
This test corrupts the handoffToken before ZoneAuthRequest.
Requires valid login ? world ? character ? enterWorld first.

Continue with full handshake? (y/n, default: y): y
Username: testuser
Password: testpass

[CLIENT] Stage: NotConnected ? LoginPending (username=testuser)
... [full handshake stages] ...

[CLIENT] Stage: CharactersLoaded ? EnteringWorld (characterId=1)

Valid handoffToken obtained: 9876543210987654321
Corrupted handoffToken:      9876543210988543209 (original + 88888)

Connecting to zone server...
[INFO] [TestClient] Sending ZoneAuthRequest with corruptedToken=9876543210988543209

[CLIENT] Stage: EnteringWorld ? InZone (Server rejected corrupted token (expected))
[INFO] [TestClient] ? Server rejected: errorCode='INVALID_HANDOFF', errorMessage='Handoff token not recognized or has expired'

? TEST PASSED: Server correctly rejected corrupted handoffToken
Error response:
  errorCode:    INVALID_HANDOFF
  errorMessage: Handoff token not recognized or has expired
Expected errorCode: INVALID_HANDOFF (stub validation accepts non-zero, future will validate properly)

Press Enter to continue...
```

### Success Criteria

- ? ZoneServer responds with ERR
- ? Error code is `INVALID_HANDOFF`
- ? Token corruption is logged
- ? Full handshake completes before corruption

### Server Logs to Check

```
[WARN] [zone] [ZONEAUTH] INVALID handoff token validation failed
[INFO] [zone] [ZONEAUTH] Sending ERROR response: payload='ERR|INVALID_HANDOFF|Handoff token not recognized or has expired'
```

**Note:** Current implementation uses stub validation that only checks for non-zero. Future implementation will validate against registry.

---

## Scenario 5: Negative Tests (Malformed Payloads)

### What It Does

Runs the original negative test suite:
- Invalid handoffToken (0)
- Malformed ZoneAuthRequest payload

### Usage

**Menu:** Select option `5`  
**Command-Line:** `REQ_TestClient.exe --negative-tests`

See `HARDENED_HANDSHAKE_GUIDE.md` for details on this scenario.

---

## Scenario 6: Interactive Mode

### What It Does

Original interactive TestClient flow with manual prompts at each stage.

### Usage

**Menu:** Select option `6`  
**Command-Line:** `REQ_TestClient.exe --interactive`

Prompts for:
- Username / password / mode (login/register)
- Character selection
- Movement commands in zone

---

## Logging Best Practices

### Client Log Format

All logs match server format for easy correlation:

```
[YYYY-MM-DD HH:MM:SS] [REQ_TestClient] [LEVEL] [TestClient] message
```

### Include Key IDs

Every log message includes relevant context:

```
[INFO] [TestClient] Selected character: id=1, name=Warrior01
[INFO] [TestClient] Zone handoff received: handoffToken=9876543210987654321, zoneId=10, endpoint=127.0.0.1:7000
```

### Stage Transitions

```
[CLIENT] Stage: WorldSelected ? CharactersLoaded (count=6)
```

### Error Logging

```
[ERROR] [TestClient] Login failed: INVALID_PASSWORD - Invalid username or password
[WARN] [TestClient] Server rejected: errorCode='INVALID_SESSION', errorMessage='Session token not recognized'
```

---

## Debugging Workflow

### Correlating Client and Server Logs

**1. Run a scenario:**
```bash
REQ_TestClient.exe --happy-path
```

**2. Grep for specific IDs:**

Find sessionToken in both logs:
```bash
grep "sessionToken=16874771734187850833" TestClient.log
grep "sessionToken=16874771734187850833" WorldServer.log
```

Find handoffToken:
```bash
grep "handoffToken=9876543210987654321" TestClient.log
grep "handoffToken=9876543210987654321" WorldServer.log
grep "handoffToken=9876543210987654321" ZoneServer.log
```

**3. Track stage progression:**
```bash
grep "\[CLIENT\] Stage:" TestClient.log
```

**4. Find errors:**
```bash
grep "ERROR" TestClient.log
grep "WARN" TestClient.log
```

### Example Debugging Session

**Problem:** Client stuck at EnteringWorld stage

**Steps:**
1. Check client logs for last stage:
   ```
   [CLIENT] Stage: EnteringWorld ? ...
   ```

2. Find handoffToken:
   ```
   handoffToken=9876543210987654321
   ```

3. Search WorldServer logs:
   ```bash
   grep "9876543210987654321" WorldServer.log
   ```
   
4. Search ZoneServer logs:
   ```bash
   grep "9876543210987654321" ZoneServer.log
   ```

5. Check for error responses:
   ```bash
   grep "ERR" ZoneServer.log
   ```

---

## Summary

**? Stage-Based Flow:**
- Clear visibility into handshake progression
- Every transition logged

**? Automated Scenarios:**
- Happy path: Full end-to-end test
- Bad password: LoginServer error handling
- Bad session: WorldServer error handling
- Bad handoff: ZoneServer error handling
- Negative tests: Malformed payloads

**? Interactive Menu:**
- Easy scenario selection
- No need to recompile for different tests

**? Command-Line Support:**
- Scriptable test execution
- CI/CD integration ready

**? Detailed Logging:**
- Matches server log format
- Includes all key IDs
- Error codes and messages

With this test harness, you can now verify the entire handshake flow without Unreal, debug issues quickly by correlating logs, and ensure all error paths are working correctly.
