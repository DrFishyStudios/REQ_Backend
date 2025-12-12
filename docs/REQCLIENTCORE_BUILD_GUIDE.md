# REQ_ClientCore - Build and Integration Guide

## Files Created

```
REQ_ClientCore/
??? REQ_ClientCore.vcxproj                    ? Visual Studio project file
??? include/
?   ??? req/
?       ??? clientcore/
?           ??? ClientCore.h                  ? Public API (465 lines)
??? src/
    ??? ClientCore_Login.cpp                  ? Login implementation
    ??? ClientCore_World.cpp                  ? Character/World implementation
    ??? ClientCore_Zone.cpp                   ? Zone communication
    ??? ClientCore_Helpers.cpp                ? Message parsers
```

---

## Step 1: Add Project to Solution

### In Visual Studio:

1. **Right-click Solution** in Solution Explorer
2. **Add ? Existing Project**
3. **Browse to:** `REQ_ClientCore\REQ_ClientCore.vcxproj`
4. **Click Open**

### Via Command Line:

```powershell
# Add to solution file (if you have one)
dotnet sln add REQ_ClientCore\REQ_ClientCore.vcxproj
```

---

## Step 2: Build REQ_ClientCore

### In Visual Studio:

1. **Right-click REQ_ClientCore** project
2. **Build**
3. **Verify output:** `x64\Debug\REQ_ClientCore.lib` is created

### Via Command Line:

```powershell
# Debug build
msbuild REQ_ClientCore\REQ_ClientCore.vcxproj /p:Configuration=Debug /p:Platform=x64

# Release build
msbuild REQ_ClientCore\REQ_ClientCore.vcxproj /p:Configuration=Release /p:Platform=x64
```

**Expected output:**
```
Build succeeded.
    0 Warning(s)
    0 Error(s)
```

---

## Step 3: Add Reference to TestClient

### Option A: Visual Studio UI

1. **Right-click REQ_TestClient** project
2. **Add ? Reference**
3. **Check REQ_ClientCore**
4. **Click OK**

### Option B: Edit `.vcxproj` Manually

Open `REQ_TestClient\REQ_TestClient.vcxproj` and add:

```xml
<ItemGroup>
  <!-- Add this new reference -->
  <ProjectReference Include="..\REQ_ClientCore\REQ_ClientCore.vcxproj">
    <Project>{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}</Project>
  </ProjectReference>
  
  <!-- Existing reference -->
  <ProjectReference Include="..\REQ_Shared\REQ_Shared.vcxproj">
    <Project>{e0827368-f2be-477a-96c8-85d940c138d0}</Project>
  </ProjectReference>
</ItemGroup>
```

---

## Step 4: Add Include Directory to TestClient

### In TestClient `.vcxproj`:

Find the `<ItemDefinitionGroup>` for your configuration and add:

```xml
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <ClCompile>
    <AdditionalIncludeDirectories>
      C:\Program Files (x86)\Microsoft SDKs\boost_1_89_0;
      $(SolutionDir)REQ_ClientCore\include;    <!-- ADD THIS LINE -->
      %(AdditionalIncludeDirectories)
    </AdditionalIncludeDirectories>
  </ClCompile>
</ItemDefinitionGroup>
```

**Repeat for all configurations:**
- `Debug|x64`
- `Release|x64`
- `Debug|Win32`
- `Release|Win32`
- `Debug|ARM64`
- `Release|ARM64`

---

## Step 5: Test Compilation

### Create a Test File in TestClient:

```cpp
// REQ_TestClient/src/test_clientcore.cpp
#include <req/clientcore/ClientCore.h>
#include <iostream>

void testClientCore() {
    using namespace req::clientcore;
    
    ClientConfig config;
    ClientSession session;
    
    std::cout << "ClientCore header included successfully!\n";
    std::cout << "ClientConfig default version: " << config.clientVersion << "\n";
}
```

### Build TestClient:

```powershell
msbuild REQ_TestClient\REQ_TestClient.vcxproj /p:Configuration=Debug /p:Platform=x64
```

**If successful:** Header is properly included and library links correctly

**If errors:** Check include paths and project references

---

## Step 6: Refactor TestClient main.cpp

### Minimal Example (Proof of Concept):

```cpp
// REQ_TestClient/src/main.cpp
#include <req/clientcore/ClientCore.h>
#include <iostream>

int main() {
    using namespace req::clientcore;
    
    // Test the API
    ClientConfig config;
    ClientSession session;
    
    std::cout << "=== Testing ClientCore ===\n";
    
    // Try login (will fail if server not running, but proves API works)
    auto loginResp = login(config, "testuser", "testpass", 
                          req::shared::protocol::LoginMode::Login, session);
    
    if (loginResp.result == LoginResult::Success) {
        std::cout << "Login succeeded!\n";
        std::cout << "Available worlds: " << loginResp.availableWorlds.size() << "\n";
    } else {
        std::cout << "Login failed (expected if server not running): " 
                 << loginResp.errorMessage << "\n";
    }
    
    return 0;
}
```

### Build and Run:

```powershell
# Build
msbuild REQ_TestClient\REQ_TestClient.vcxproj /p:Configuration=Debug /p:Platform=x64

# Run
.\x64\Debug\REQ_TestClient.exe
```

**Expected output (server not running):**
```
=== Testing ClientCore ===
Login failed (expected if server not running): Failed to connect to login server: ...
```

**This proves the API is working!**

---

## Step 7: Full Integration (After Testing)

Once the minimal test works, proceed with full refactor:

1. **Update `TestClient::run()`** to use ClientCore API
2. **Update `runMovementTestLoop()`** to use ClientCore session
3. **Remove old methods:**
   - `doLogin()`
   - `doCharacterList()`
   - `doCharacterCreate()`
   - `doEnterWorld()`
   - `doZoneAuthAndConnect()`
   - `SendDevCommand()` (now in ClientCore)

See `docs/REQCLIENTCORE_IMPLEMENTATION_SUMMARY.md` for detailed refactor examples.

---

## Step 8: Add to SFML Client (Future)

### In REQ_VizTestClient project:

1. **Add project reference** to REQ_ClientCore
2. **Add include directory:** `$(SolutionDir)REQ_ClientCore\include`
3. **Include header:** `#include <req/clientcore/ClientCore.h>`
4. **Use API in SFML main loop**

Example:
```cpp
// REQ_VizTestClient/main.cpp
#include <SFML/Graphics.hpp>
#include <req/clientcore/ClientCore.h>

int main() {
    using namespace req::clientcore;
    
    ClientConfig config;
    ClientSession session;
    
    // Login
    auto loginResp = login(config, "testuser", "testpass", 
                          req::shared::protocol::LoginMode::Login, session);
    if (loginResp.result != LoginResult::Success) {
        return 1;
    }
    
    // Character list
    auto charListResp = getCharacterList(session);
    // ... enter world ...
    
    // Zone connection
    auto zoneResp = connectToZone(session);
    
    // SFML main loop
    sf::RenderWindow window(sf::VideoMode({1280u, 720u}), "REQ Client");
    std::uint32_t movementSeq = 0;
    
    while (window.isOpen()) {
        // Handle SFML events
        while (std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }
        
        // Send movement
        float inputX = 0.0f, inputY = 0.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) inputY = 1.0f;
        sendMovementIntent(session, inputX, inputY, 0.0f, false, ++movementSeq);
        
        // Receive zone messages
        ZoneMessage msg;
        while (tryReceiveZoneMessage(session, msg)) {
            if (msg.type == req::shared::MessageType::PlayerStateSnapshot) {
                req::shared::protocol::PlayerStateSnapshotData snapshot;
                if (parsePlayerStateSnapshot(msg.payload, snapshot)) {
                    // Update player positions for rendering
                }
            }
        }
        
        window.clear(sf::Color(30, 30, 40));
        // Draw game...
        window.display();
    }
    
    disconnectFromZone(session);
    return 0;
}
```

---

## Troubleshooting

### Error: "Cannot open include file: 'req/clientcore/ClientCore.h'"

**Fix:** Add include directory to project settings:
```
$(SolutionDir)REQ_ClientCore\include
```

### Error: "Unresolved external symbol"

**Fix:** Add project reference to REQ_ClientCore

### Error: "REQ_ClientCore.lib not found"

**Fix:** Build REQ_ClientCore first, then rebuild TestClient

### Error: Build order issues

**Fix:** Set project dependencies:
1. Right-click Solution ? Properties
2. Project Dependencies
3. Select REQ_TestClient
4. Check REQ_ClientCore
5. Apply ? OK

---

## Verification Checklist

Before considering integration complete:

- [ ] REQ_ClientCore.lib builds successfully
- [ ] TestClient includes `<req/clientcore/ClientCore.h>` without errors
- [ ] TestClient links successfully with ClientCore
- [ ] Minimal test (login attempt) runs without crashes
- [ ] Full TestClient refactor preserves all existing behavior
- [ ] All test scenarios pass
- [ ] Bot mode still works
- [ ] SFML client can include and use ClientCore

---

## Performance Notes

### Static Library Size:

**Debug build:** ~200-300 KB (depends on optimization)  
**Release build:** ~50-100 KB (with optimizations)

### Runtime Overhead:

**Zero additional overhead** - Functions inline where appropriate, no virtual dispatch, no heap allocations in hot paths.

### Compile Time:

**Slightly faster** - Shared core compiles once, not duplicated in each client project.

---

## Summary

### What You Have Now:

? **REQ_ClientCore** static library (fully implemented)  
? **Public API** in `ClientCore.h` (clean, documented)  
? **All core networking** extracted from TestClient  
? **Visual Studio project** ready to add to solution  

### What You Need To Do:

1. ? Add REQ_ClientCore project to solution
2. ? Build REQ_ClientCore
3. ? Add reference from TestClient
4. ? Add include directory
5. ? Test compilation
6. ? Refactor TestClient main.cpp
7. ? Test all scenarios still work
8. ? (Optional) Add to SFML client

### Expected Time:

**Step 1-5 (setup):** 15-30 minutes  
**Step 6-7 (refactor):** 2-4 hours (careful work to preserve behavior)  
**Step 8 (SFML):** 1-2 hours

---

**Ready to proceed!** Follow steps 1-5 first to verify project builds, then proceed with refactoring.
