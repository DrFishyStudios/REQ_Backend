# TestClient Smoke Test for REQ_ClientCore

## Status
? **Smoke test file created:** `REQ_TestClient/src/TestClientCoreSmokeTest.cpp`

## What Still Needs to Be Done

The REQ_TestClient project needs two configuration changes to properly link with REQ_ClientCore:

### 1. Add Project Reference to REQ_ClientCore

**In Visual Studio:**
1. Right-click **REQ_TestClient** project in Solution Explorer
2. Click **Add ? Reference...**
3. Check the box next to **REQ_ClientCore**
4. Click **OK**

**Or manually edit `REQ_TestClient.vcxproj`:**

Find this section:
```xml
<ItemGroup>
  <ProjectReference Include="..\REQ_Shared\REQ_Shared.vcxproj">
    <Project>{e0827368-f2be-477a-96c8-85d940c138d0}</Project>
  </ProjectReference>
</ItemGroup>
```

Change it to:
```xml
<ItemGroup>
  <ProjectReference Include="..\REQ_ClientCore\REQ_ClientCore.vcxproj">
    <Project>{b4c7d8a1-9f2e-4a3b-8c5d-1e6f7a8b9c0d}</Project>
  </ProjectReference>
  <ProjectReference Include="..\REQ_Shared\REQ_Shared.vcxproj">
    <Project>{e0827368-f2be-477a-96c8-85d940c138d0}</Project>
  </ProjectReference>
</ItemGroup>
```

---

### 2. Add Include Directory for REQ_ClientCore

**You need to add** `$(SolutionDir)REQ_ClientCore\include` to the `AdditionalIncludeDirectories` for all configurations.

**In Visual Studio:**
1. Right-click **REQ_TestClient** project
2. Click **Properties**
3. Go to **C/C++ ? General**
4. Edit **Additional Include Directories**
5. Add: `$(SolutionDir)REQ_ClientCore\include`
6. Click **OK**
7. Repeat for all configurations (Debug/Release × Win32/x64/ARM64)

**Or manually edit `REQ_TestClient.vcxproj`:**

For each `<ItemDefinitionGroup>` (there are 6 configurations), change:
```xml
<AdditionalIncludeDirectories>C:\Program Files (x86)\Microsoft SDKs\boost_1_89_0</AdditionalIncludeDirectories>
```

To:
```xml
<AdditionalIncludeDirectories>C:\Program Files (x86)\Microsoft SDKs\boost_1_89_0;$(SolutionDir)REQ_ClientCore\include</AdditionalIncludeDirectories>
```

---

### 3. Add Smoke Test File to Project

The file `TestClientCoreSmokeTest.cpp` has been created, but Visual Studio needs to know about it.

**In Visual Studio:**
1. Right-click **REQ_TestClient** project ? **Add ? Existing Item...**
2. Navigate to `REQ_TestClient\src\`
3. Select `TestClientCoreSmokeTest.cpp`
4. Click **Add**

**Or manually edit `REQ_TestClient.vcxproj`:**

Find the `<ItemGroup>` with all the `<ClCompile>` entries and add:
```xml
<ClCompile Include="src\TestClientCoreSmokeTest.cpp" />
```

---

## Verification Steps

After making these changes:

### Step 1: Rebuild REQ_TestClient
```powershell
msbuild REQ_TestClient\REQ_TestClient.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Rebuild
```

### Step 2: Check for Success
**Expected output:**
```
Build succeeded.
    0 Warning(s)
    0 Error(s)
```

**If you see errors like:**
- `Cannot open include file: 'req/clientcore/ClientCore.h'` ? Missing include directory (Step 2)
- `Unresolved external symbol` ? Missing project reference (Step 1)
- `'TestClientCoreSmokeTest.cpp': No such file or directory` ? Missing file in project (Step 3)

---

## What the Smoke Test Does

The smoke test file (`TestClientCoreSmokeTest.cpp`) contains a simple function `TestClientCoreSmoke()` that:

1. ? **Includes** the ClientCore header
2. ? **Instantiates** `ClientConfig` and `ClientSession` types
3. ? **Accesses** a field (`config.clientVersion`)
4. ? **Prints** a message

**Important:** The function is **not called** from `main()` yet. This is purely a **compilation and linkage test**.

If the project builds successfully, it proves:
- ? The include path is correct
- ? The types are visible
- ? The library links properly

---

## Next Steps (After Verification)

Once the smoke test compiles and links successfully:

1. **Optional:** Add a call to `TestClientCoreSmoke()` in `main()` to see the output
2. **Start refactoring:** Begin replacing TestClient's internal networking code with ClientCore API calls
3. **Follow the guide:** See `docs/REQCLIENTCORE_IMPLEMENTATION_SUMMARY.md` for refactoring examples

---

## Quick Reference Commands

### Rebuild TestClient (Command Line)
```powershell
# Debug build
msbuild REQ_TestClient\REQ_TestClient.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Rebuild

# Release build
msbuild REQ_TestClient\REQ_TestClient.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Rebuild
```

### Run TestClient (after build succeeds)
```powershell
.\x64\Debug\REQ_TestClient.exe
```

---

## Summary

? **Created:** `REQ_TestClient/src/TestClientCoreSmokeTest.cpp`  
? **TODO:** Add project reference to REQ_ClientCore  
? **TODO:** Add include directory to project settings  
? **TODO:** Add smoke test file to project  
? **TODO:** Rebuild and verify  

**Expected time:** 5 minutes for configuration + 1 minute for build verification

---

**Status:** Smoke test file ready, awaiting project configuration changes! ??
