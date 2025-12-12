# Add REQ_ClientCore to REQ_Backend.sln
# Run this script from the solution root directory

$solutionFile = "REQ_Backend.sln"

Write-Host "Adding REQ_ClientCore to $solutionFile..." -ForegroundColor Cyan

# Read the solution file
$content = Get-Content $solutionFile -Raw

# Check if already added
if ($content -match "REQ_ClientCore") {
    Write-Host "REQ_ClientCore is already in the solution!" -ForegroundColor Yellow
    exit 0
}

# Project declaration to add (after REQ_VizTestClient)
$projectDeclaration = @"
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "REQ_ClientCore", "REQ_ClientCore\REQ_ClientCore.vcxproj", "{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}"
EndProject
"@

# Build configurations to add (before EndGlobalSection)
$buildConfigs = @"
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Debug|ARM64.ActiveCfg = Debug|ARM64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Debug|ARM64.Build.0 = Debug|ARM64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Debug|x64.ActiveCfg = Debug|x64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Debug|x64.Build.0 = Debug|x64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Debug|x86.ActiveCfg = Debug|Win32
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Debug|x86.Build.0 = Debug|Win32
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Release|ARM64.ActiveCfg = Release|ARM64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Release|ARM64.Build.0 = Release|ARM64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Release|x64.ActiveCfg = Release|x64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Release|x64.Build.0 = Release|x64
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Release|x86.ActiveCfg = Release|Win32
		{B4C7D8A1-9F2E-4A3B-8C5D-1E6F7A8B9C0D}.Release|x86.Build.0 = Release|Win32
"@

# Step 1: Add project declaration
$content = $content -replace "(REQ_VizTestClient.*?EndProject)", "`$1`r`n$projectDeclaration"

# Step 2: Add build configurations
$content = $content -replace "(\{BF042EF8-A9AE-49B4-94AF-F3C25DDDF178\}\.Release\|x86\.Build\.0 = Release\|Win32)`r`n(\t+EndGlobalSection)", "`$1`r`n$buildConfigs`r`n`$2"

# Write back
Set-Content $solutionFile -Value $content -NoNewline

Write-Host "? Successfully added REQ_ClientCore to solution!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Open REQ_Backend.sln in Visual Studio"
Write-Host "2. Right-click REQ_ClientCore project ? Build"
Write-Host "3. Verify output: x64\Debug\REQ_ClientCore.lib is created"
Write-Host ""
Write-Host "Then follow the build guide to add references to TestClient and VizTestClient." -ForegroundColor Cyan
