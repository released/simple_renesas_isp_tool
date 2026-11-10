param(
    [string]$VersionFile = "",
    [string]$HeaderFile = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($VersionFile)) {
    $VersionFile = Join-Path $repoRoot "src\\build_version.txt"
}
if ([string]::IsNullOrWhiteSpace($HeaderFile)) {
    $HeaderFile = Join-Path $repoRoot "src\\build_info.generated.h"
}

if (-not (Test-Path -LiteralPath $VersionFile)) {
    [System.IO.File]::WriteAllText($VersionFile, "0.1.0.0000", [System.Text.Encoding]::ASCII)
}

$raw = [System.IO.File]::ReadAllText($VersionFile).Trim()
$m = [System.Text.RegularExpressions.Regex]::Match($raw, '^(\d+)\.(\d+)\.(\d+)\.(\d+)$')
if (-not $m.Success) {
    throw "Invalid version format in ${VersionFile}: $raw"
}

$major = [int]$m.Groups[1].Value
$minor = [int]$m.Groups[2].Value
$patch = [int]$m.Groups[3].Value
$build = [int]$m.Groups[4].Value

$build++
if ($build -gt 999) { $build=1; $patch++ }
if ($patch -gt 9) { $patch=0; $minor++ }
if ($minor -gt 9) { $minor=0; $major++ }
if ($major -gt 9) { throw 'Version overflow; the previous counter is preserved.' }
$newVersion = "{0}.{1}.{2}.{3:D3}" -f $major, $minor, $patch, $build

[System.IO.File]::WriteAllText($VersionFile, $newVersion + [Environment]::NewLine, [System.Text.Encoding]::ASCII)

& (Join-Path $PSScriptRoot 'prepare_tool_build_identity.ps1') -Version $newVersion -OutputHeader (Join-Path (Split-Path -Parent $HeaderFile) 'tool_build_identity.h')
$header = @"
#pragma once
#include "tool_build_identity.h"
#define FIRMWARE_UPGRADE_TOOL_VERSION TOOL_BUILD_VERSION_W
#define FIRMWARE_UPGRADE_TOOL_BUILD_TIME TOOL_BUILD_DISPLAY_TIME_W
"@
[System.IO.File]::WriteAllText($HeaderFile, $header, [System.Text.Encoding]::ASCII)

Write-Host "[OK] Version bumped: $newVersion"
