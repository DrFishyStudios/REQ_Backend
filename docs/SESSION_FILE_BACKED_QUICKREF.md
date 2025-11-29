# File-Backed Session Service - Quick Reference

## TL;DR

SessionService now shares sessions across LoginServer and WorldServer processes using `data/sessions.json`.

---

## What Changed

### SessionService (REQ_Shared)

**New API:**
```cpp
// Configure file-backed persistence
sessionService.configure("data/sessions.json");

// Save sessions to file
sessionService.saveToFile();

// Load sessions from file
sessionService.loadFromFile();
```

---

## LoginServer

**main.cpp:**
```cpp
// Configure SessionService on startup
auto& sessionService = req::shared::SessionService::instance();
sessionService.configure("data/sessions.json");
```

**LoginServer.cpp:**
```cpp
// After creating session
auto token = sessionService.createSession(accountId);
sessionService.saveToFile();  // ? NEW: Save to file for WorldServer
```

---

## WorldServer

**main.cpp:**
```cpp
// Configure SessionService on startup
auto& sessionService = req::shared::SessionService::instance();
sessionService.configure("data/sessions.json");
```

**WorldServer.cpp (resolveSessionToken):**
```cpp
// First attempt
auto session = sessionService.validateSession(token);

if (!session.has_value()) {
    // Reload-on-miss: Try loading from file
    sessionService.loadFromFile();
    
    // Second attempt
    session = sessionService.validateSession(token);
}

if (session.has_value()) {
    return session->accountId;
}

return std::nullopt;
```

---

## Flow

```
1. Client logs in to LoginServer
   ?
2. LoginServer creates session: sessionService.createSession(accountId)
   ?
3. LoginServer saves to file: sessionService.saveToFile()
   ? Writes data/sessions.json
   
4. Client sends CharacterListRequest to WorldServer
   ?
5. WorldServer validates session: sessionService.validateSession(token)
   ? Not in memory (first time)
   ?
6. WorldServer reloads: sessionService.loadFromFile()
   ? Reads data/sessions.json
   ?
7. WorldServer validates again: sessionService.validateSession(token)
   ? Found! Returns accountId
   ?
8. WorldServer processes request successfully
```

---

## File Format

**Path:** `data/sessions.json`

```json
{
  "sessions": [
    {
      "sessionToken": 9903440321268838814,
      "accountId": 1,
      "createdAt": "2024-11-29T14:32:15",
      "lastSeen": "2024-11-29T14:35:22",
      "boundWorldId": 1
    }
  ]
}
```

---

## Expected Logs

### LoginServer
```
[INFO] [Main] Configuring SessionService with file: data/sessions.json
[INFO] [SessionService] Session created: accountId=1, sessionToken=9903440321268838814
[INFO] [SessionService] Sessions saved to file: path=data/sessions.json, count=1
```

### WorldServer (First Request)
```
[INFO] [Main] Configuring SessionService with file: data/sessions.json
[INFO] [SessionService] Configured with file 'data/sessions.json', loaded 0 session(s)
[INFO] [world] Session not in memory, reloading from file: sessionToken=9903440321268838814
[INFO] [SessionService] Sessions loaded from file: path=data/sessions.json, count=1
[INFO] [SessionService] Session validated: sessionToken=9903440321268838814, accountId=1
[INFO] [world] Session found after reload
```

### WorldServer (Subsequent Requests)
```
[INFO] [SessionService] Session validated: sessionToken=9903440321268838814, accountId=1
```

---

## Testing

**1. Stop all servers**
**2. Build solution**
**3. Start servers:**
```powershell
# Terminal 1
cd x64\Debug
.\REQ_LoginServer.exe

# Terminal 2
.\REQ_WorldServer.exe

# Terminal 3
.\REQ_ZoneServer.exe --zone_name="East Freeport"
```

**4. Run client (Unreal or TestClient)**

**5. Verify:**
- Login succeeds
- Character list works (not INVALID_SESSION error)
- File `data/sessions.json` created
- WorldServer logs show "Session found after reload"

---

## Troubleshooting

### "INVALID_SESSION" Error

**Check:**
1. Both servers configured with same file path
2. File `data/sessions.json` exists
3. LoginServer logs show "Sessions saved to file"
4. WorldServer logs show "Sessions loaded from file"

**Fix:**
- Restart servers
- Delete `data/sessions.json` and re-login

### "Session file not found"

**Expected:** File created on first login - not an error

### Build Errors

**Issue:** LNK1168 - cannot open exe for writing

**Cause:** Servers still running

**Fix:** Stop all servers before building

---

## Summary

? SessionService supports file-backed persistence  
? LoginServer saves sessions after creation  
? WorldServer reloads on cache miss  
? Cross-process session sharing works  
? No Redis required for dev/test environments  

**Problem SOLVED** ??
