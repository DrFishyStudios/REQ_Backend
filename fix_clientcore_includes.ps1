# Fix includes in REQ_ClientCore files
# This script fixes relative path includes to use proper include directories

Write-Host "Fixing includes in REQ_ClientCore files..." -ForegroundColor Cyan

# Fix implementation files - they should use req/clientcore/ClientCore.h
$files = @(
    "REQ_ClientCore\src\ClientCore_Login.cpp",
    "REQ_ClientCore\src\ClientCore_World.cpp",
    "REQ_ClientCore\src\ClientCore_Zone.cpp",
    "REQ_ClientCore\src\ClientCore_Helpers.cpp"
)

foreach ($file in $files) {
    if (Test-Path $file) {
        Write-Host "Processing $file..." -ForegroundColor Yellow
        
        $content = Get-Content $file -Raw
        
        # Fix main header include
        $content = $content -replace '#include "\.\./include/req/clientcore/ClientCore\.h"', '#include "req/clientcore/ClientCore.h"'
        
        # Fix REQ_Shared includes
        $content = $content -replace '#include "\.\./\.\./REQ_Shared/include/(req/shared/[^"]+)"', '#include "REQ_Shared/$1"'
        
        Set-Content $file -Value $content -NoNewline
        Write-Host "  ? Fixed includes in $file" -ForegroundColor Green
    }
}

# Fix project file - add REQ_Shared include directory
$vcxproj = "REQ_ClientCore\REQ_ClientCore.vcxproj"
if (Test-Path $vcxproj) {
    Write-Host "Processing $vcxproj..." -ForegroundColor Yellow
    
    $content = Get-Content $vcxproj -Raw
    
    # Add REQ_Shared include to all configurations
    $oldInclude = ';`$(ProjectDir)include</AdditionalIncludeDirectories>'
    $newInclude = ';`$(ProjectDir)include;`$(SolutionDir)REQ_Shared\include</AdditionalIncludeDirectories>'
    
    $content = $content -replace [regex]::Escape($oldInclude), $newInclude
    
    Set-Content $vcxproj -Value $content -NoNewline
    Write-Host "  ? Fixed includes in $vcxproj" -ForegroundColor Green
}

Write-Host ""
Write-Host "? All includes fixed!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Reload the solution in Visual Studio"
Write-Host "2. Build REQ_ClientCore project"
Write-Host "3. Verify no include errors"
