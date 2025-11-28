# REQ Backend End-to-End Handshake Testing Guide

## Overview
This guide explains how to test the complete login ? world ? zone handshake flow using the updated servers and test client.

## Prerequisites
- All projects successfully built (REQ_Shared, REQ_LoginServer, REQ_WorldServer, REQ_ZoneServer, REQ_TestClient)
- Configuration files in `config/` directory:
  - `login_config.json`
  - `world_config.json`

## Step-by-Step Testing

### 1. Start Login Server
```powershell
cd x64\Debug
.\REQ_LoginServer.exe
```

**Expected Output:**
```
[timestamp] [REQ_LoginServer] [INFO] [Main] Loading configuration...
[timestamp] [REQ_LoginServer] [INFO] [Config] Loading LoginConfig from: config/login_config.json
[timestamp] [REQ_LoginServer] [INFO] [Config] LoginConfig loaded: address=0.0.0.0, port=7777
[timestamp] [REQ_LoginServer] [INFO] [Main] Configuration loaded successfully: address=0.0.0.0, port=7777
[timestamp] [REQ_LoginServer] [INFO] [login] LoginServer starting on 0.0.0.0:7777
[timestamp] [REQ_LoginServer] [INFO] [login] MOTD: Welcome to REQ Backend - Login Server
```

### 2. Start World Server (in new terminal)
```powershell
cd x64\Debug
.\REQ_WorldServer.exe
```

**Expected Output:**
```
[timestamp] [REQ_WorldServer] [INFO] [Main] Loading configuration...
[timestamp] [REQ_WorldServer] [INFO] [Config] Loading WorldConfig from: config/world_config.json
[timestamp] [REQ_WorldServer] [INFO] [Config] WorldConfig loaded: worldId=1, worldName=MainWorld, ...
[timestamp] [REQ_WorldServer] [INFO] [Main] Configuration loaded successfully:
[timestamp] [REQ_WorldServer] [INFO] [Main]   worldId=1
[timestamp] [REQ_WorldServer] [INFO] [Main]   worldName=MainWorld
[timestamp] [REQ_WorldServer] [INFO] [Main]   address=0.0.0.0:7778
[timestamp] [REQ_WorldServer] [INFO] [Main]   zones=2
[timestamp] [REQ_WorldServer] [INFO] [Main]   autoLaunchZones=false
[timestamp] [REQ_WorldServer] [INFO] [world] WorldServer starting: worldId=1, worldName=MainWorld
[timestamp] [REQ_WorldServer] [INFO] [world] Listening on 0.0.0.0:7778
[timestamp] [REQ_WorldServer] [INFO] [world] Ruleset: standard, zones=2, autoLaunchZones=false
```

### 3. Start Zone Server (in new terminal)
```powershell
cd x64\Debug
.\REQ_ZoneServer.exe
```

**Expected Output:**
```
[timestamp] [REQ_ZoneServer] [INFO] [Main] Starting ZoneServer...
[timestamp] [REQ_ZoneServer] [INFO] [Main]   zoneId=1
[timestamp] [REQ_ZoneServer] [INFO] [Main]   address=0.0.0.0:7779
[timestamp] [REQ_ZoneServer] [INFO] [zone] ZoneServer starting: zoneId=1
```

### 4. Run Test Client (in new terminal)
```powershell
cd x64\Debug
.\REQ_TestClient.exe
```

**Expected Output:**
```
[timestamp] [REQ_TestClient] [INFO] [TestClient] === Starting Login ? World ? Zone Handshake Test ===
[timestamp] [REQ_TestClient] [INFO] [TestClient] --- Stage 1: Login ---
[timestamp] [REQ_TestClient] [INFO] [TestClient] Connecting to login server at 127.0.0.1:7777...
[timestamp] [REQ_TestClient] [INFO] [TestClient] Connected to login server
[timestamp] [REQ_TestClient] [INFO] [TestClient] Sending LoginRequest: username=testuser, clientVersion=0.1.0
[timestamp] [REQ_TestClient] [INFO] [TestClient] Received: type=11, protocolVersion=1, payloadSize=...
[timestamp] [REQ_TestClient] [INFO] [TestClient] Selected world: MainWorld (ruleset: standard)
[timestamp] [REQ_TestClient] [INFO] [TestClient] Login stage succeeded:
[timestamp] [REQ_TestClient] [INFO] [TestClient]   sessionToken=...
[timestamp] [REQ_TestClient] [INFO] [TestClient]   worldId=1
[timestamp] [REQ_TestClient] [INFO] [TestClient]   worldEndpoint=127.0.0.1:7778
[timestamp] [REQ_TestClient] [INFO] [TestClient] --- Stage 2: World Auth ---
[timestamp] [REQ_TestClient] [INFO] [TestClient] Connecting to world server at 127.0.0.1:7778...
[timestamp] [REQ_TestClient] [INFO] [TestClient] Connected to world server
[timestamp] [REQ_TestClient] [INFO] [TestClient] Sending WorldAuthRequest: sessionToken=..., worldId=1
[timestamp] [REQ_TestClient] [INFO] [TestClient] Received: type=21, protocolVersion=1, payloadSize=...
[timestamp] [REQ_TestClient] [INFO] [TestClient] World auth stage succeeded:
[timestamp] [REQ_TestClient] [INFO] [TestClient]   handoffToken=...
[timestamp] [REQ_TestClient] [INFO] [TestClient]   zoneId=1
[timestamp] [REQ_TestClient] [INFO] [TestClient]   zoneEndpoint=127.0.0.1:7779
[timestamp] [REQ_TestClient] [INFO] [TestClient] --- Stage 3: Zone Auth ---
[timestamp] [REQ_TestClient] [INFO] [TestClient] Connecting to zone server at 127.0.0.1:7779...
[timestamp] [REQ_TestClient] [INFO] [TestClient] Connected to zone server
[timestamp] [REQ_TestClient] [INFO] [TestClient] Sending ZoneAuthRequest: handoffToken=..., characterId=42
[timestamp] [REQ_TestClient] [INFO] [TestClient] Received: type=31, protocolVersion=1, payloadSize=...
[timestamp] [REQ_TestClient] [INFO] [TestClient] Zone entry successful: Welcome to zone 1 (character 42)
[timestamp] [REQ_TestClient] [INFO] [TestClient] === Full Handshake Completed Successfully ===
```

### Server-Side Logs

**Login Server:**
```
[timestamp] [REQ_LoginServer] [INFO] [login] New client connected
[timestamp] [REQ_LoginServer] [INFO] [login] Received message: type=10, protocolVersion=1, payloadSize=...
[timestamp] [REQ_LoginServer] [INFO] [login] LoginRequest: username=testuser, clientVersion=0.1.0
[timestamp] [REQ_LoginServer] [INFO] [login] LoginResponse OK: username=testuser, sessionToken=..., worldCount=1
```

**World Server:**
```
[timestamp] [REQ_WorldServer] [INFO] [world] New client connected
[timestamp] [REQ_WorldServer] [INFO] [world] Received message: type=20, protocolVersion=1, payloadSize=...
[timestamp] [REQ_WorldServer] [INFO] [world] WorldAuthRequest: sessionToken=..., worldId=1
[timestamp] [REQ_WorldServer] [INFO] [world] WorldAuthResponse OK: handoffToken=..., zoneId=1, endpoint=127.0.0.1:7779
```

**Zone Server:**
```
[timestamp] [REQ_ZoneServer] [INFO] [zone] New client connected
[timestamp] [REQ_ZoneServer] [INFO] [zone] Received message: type=30, protocolVersion=1, payloadSize=...
[timestamp] [REQ_ZoneServer] [INFO] [zone] ZoneAuthRequest: handoffToken=..., characterId=42
[timestamp] [REQ_ZoneServer] [INFO] [zone] ZoneAuthResponse OK: characterId=42 entered zone 1
```

## Verification Checklist

? All servers start without errors
? Configuration files load successfully
? Protocol version (1) is logged for all messages
? Login stage completes with session token generation
? World list is returned with correct world info
? World auth stage completes with handoff token generation
? Zone selection from config works
? Zone auth stage completes with welcome message
? All message types use ProtocolSchemas helpers
? Error handling logs clear messages for failures

## Testing Error Cases

### Invalid Username (empty)
Modify TestClient username to empty string. Should see:
```
[ERROR] Login failed: AUTH_FAILED - Username cannot be empty
```

### Wrong World ID
Modify TestClient to request worldId=999. Should see:
```
[ERROR] World auth failed: WRONG_WORLD - This world server does not match requested worldId
```

### Invalid Tokens
Use token value 0. Should see appropriate "INVALID_SESSION" or "INVALID_HANDOFF" errors.

## Next Steps

After successful testing:
1. Implement real session validation (shared session service)
2. Implement handoff token validation between world/zone
3. Add database integration for accounts and characters
4. Implement character selection
5. Add world list from database/registry service
6. Implement zone load balancing
