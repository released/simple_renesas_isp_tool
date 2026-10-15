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
$newVersion = "{0}.{1}.{2}.{3:D4}" -f $major, $minor, $patch, $build
$buildTime = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")

[System.IO.File]::WriteAllText($VersionFile, $newVersion + [Environment]::NewLine, [System.Text.Encoding]::ASCII)

$header = @"
#pragma once

#define FIRMWARE_UPGRADE_TOOL_VERSION L"$newVersion"
#define FIRMWARE_UPGRADE_TOOL_BUILD_TIME L"$buildTime"
"@
[System.IO.File]::WriteAllText($HeaderFile, $header, [System.Text.Encoding]::ASCII)

Write-Host "[OK] Version bumped: $newVersion ($buildTime)"
