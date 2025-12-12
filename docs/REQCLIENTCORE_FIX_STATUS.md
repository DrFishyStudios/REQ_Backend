# REQ_ClientCore Include Fix - Status

## ? What's Been Fixed

### 1. Header File (`ClientCore.h`)
**Status:** ? **COMPLETE**

Changed from:
```cpp
#include "../../../REQ_Shared/include/req/shared/Types.h"
```

To:
```cpp
#include "REQ_Shared/req/shared/Types.h"
```

### 2. Implementation Files (All `.cpp` files)
**Status:** ? **COMPLETE**

All 4 implementation files have been fixed:
- ? `ClientCore_Login.cpp`
- ? `ClientCore_World.cpp`
- ? `ClientCore_Zone.cpp`
- ? `ClientCore_Helpers.cpp`

Changed from:
```cpp
#include "../include/req/clientcore/ClientCore.h"
#include "../../REQ_Shared/include/req/shared/MessageHeader.h"
```

To:
```cpp
#include "req/clientcore/ClientCore.h"
#include "REQ_Shared/req/shared/MessageHeader.h"
```

---

## ? What Still Needs to Be Done

### Project File (`REQ_ClientCore.vcxproj`)
**Status:** ? **NEEDS MANUAL EDIT**

The project file is currently open in Visual Studio, so I couldn't modify it automatically.

**You need to add** `$(SolutionDir)REQ_Shared\include` **to the include directories.**

---

## Manual Fix Instructions

### Step 1: Close the Project File

1. In Visual Studio, **close** `REQ_ClientCore.vcxproj` (if you have it open as text)
2. Or just **close Visual Studio** entirely

### Step 2: Edit the Project File

Open `REQ_ClientCore\REQ_ClientCore.vcxproj` in a text editor (Notepad, VS Code, etc.)

### Step 3: Find and Replace

**Find this line** (appears 6 times - once per configuration):
```xml
<AdditionalIncludeDirectories>C:\Program Files (x86)\Microsoft SDKs\boost_1_89_0;$(ProjectDir)include</AdditionalIncludeDirectories>
```

**Replace with:**
```xml
<AdditionalIncludeDirectories>C:\Program Files (x86)\Microsoft SDKs\boost_1_89_0;$(ProjectDir)include;$(SolutionDir)REQ_Shared\include</AdditionalIncludeDirectories>
```

**You need to change it in all 6 configurations:**
- Debug|Win32
- Release|Win32
- Debug|x64
- Release|x64
- Debug|ARM64
- Release|ARM64

### Step 4: Save and Reload

1. **Save** the `.vcxproj` file
2. **Reopen** Visual Studio
3. Visual Studio will prompt to reload the project - click **Yes**

---

## Alternative: Use Search and Replace in VS

If you prefer using Visual Studio:

1. **Right-click** `REQ_ClientCore.vcxproj` ? **Open With** ? **XML (Text) Editor**
2. Press **Ctrl+H** (Find and Replace)
3. **Find:** `;$(ProjectDir)include</AdditionalIncludeDirectories>`
4. **Replace:** `;$(ProjectDir)include;$(SolutionDir)REQ_Shared\include</AdditionalIncludeDirectories>`
5. Click **Replace All**
6. **Save** the file
7. **Reload** the project

---

## Verify the Fix

After making the change, build the project:

```powershell
msbuild REQ_ClientCore\REQ_ClientCore.vcxproj /p:Configuration=Debug /p:Platform=x64
```

**Expected result:** Build succeeds with 0 errors

**If you see include errors:**
- Double-check that you added the include directory to **all 6 configurations**
- Make sure the path is: `$(SolutionDir)REQ_Shared\include` (with backslash, not forward slash)

---

## Summary of Changes Needed

**Before:**
```xml
<AdditionalIncludeDirectories>C:\Program Files (x86)\Microsoft SDKs\boost_1_89_0;$(ProjectDir)include</AdditionalIncludeDirectories>
```

**After:**
```xml
<AdditionalIncludeDirectories>C:\Program Files (x86)\Microsoft SDKs\boost_1_89_0;$(ProjectDir)include;$(SolutionDir)REQ_Shared\include</AdditionalIncludeDirectories>
```

Just add `;$(SolutionDir)REQ_Shared\include` to the end of each `<AdditionalIncludeDirectories>` line.

---

## Current Status

? **Code files:** All fixed  
? **Project file:** Needs manual edit (file is open in VS)  

**Once you make this one change, REQ_ClientCore should build successfully!** ??
