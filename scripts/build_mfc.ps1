param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [bool]$CloseRunningTool = $true
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$sln = Join-Path $repoRoot "FirmwareUpgradeTool.sln"
$bumpScript = Join-Path $PSScriptRoot "bump_version.ps1"
$compiledExe = if ($Configuration -eq "Debug") {
    Join-Path $repoRoot ("tmp\internal\build\debug\{0}\FirmwareUpgradeTool.exe" -f $Platform)
} else {
    Join-Path $repoRoot ("tmp\internal\build\release\{0}\FirmwareUpgradeTool.exe" -f $Platform)
}
$deliveryExe = Join-Path $repoRoot "build\FirmwareUpgradeTool.exe"
$toolExe = if ($Configuration -eq "Release" -and $Platform -eq "x64") {
    $deliveryExe
} else {
    $compiledExe
}

if ($CloseRunningTool) {
    . (Join-Path $PSScriptRoot 'close_project_tool.ps1')
    Close-ProjectToolForBuild -BuildDirectory (Join-Path $repoRoot 'build')
    Close-ProjectToolForBuild -BuildDirectory (Join-Path $repoRoot 'tmp\internal\build')
}

$msbuild = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
}
if (-not (Test-Path $msbuild)) {
    $msbuild = "msbuild"
}

if ($Configuration -eq "Release" -and (Test-Path $bumpScript)) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $bumpScript
    if ($LASTEXITCODE -ne 0) {
        throw "Version bump failed."
    }
}

Write-Host "Building $sln ($Configuration|$Platform)"
& $msbuild $sln /t:Build /p:Configuration=$Configuration /p:Platform=$Platform /m
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

if (-not (Test-Path -LiteralPath $compiledExe)) {
    throw "Build output was not found: $compiledExe"
}

if ($Configuration -eq "Release" -and $Platform -eq "x64") {
    Copy-Item -LiteralPath $compiledExe -Destination $deliveryExe -Force
    Write-Host "Release delivery copy updated: $deliveryExe"
}

Write-Host "Build success. Output: $toolExe"
