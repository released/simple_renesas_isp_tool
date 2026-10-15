# Companion repositories default to siblings; overrides are machine-local.
function Get-CompanionPath {
    param(
        [ValidateSet('F13Boot','F13App','Web')][string]$Kind,
        [string]$ExplicitPath,
        [string]$ToolRoot = (Split-Path -Parent $PSScriptRoot)
    )
    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return [IO.Path]::GetFullPath($ExplicitPath)
    }
    $variable = switch ($Kind) {
        'F13Boot' { 'FIRMWARE_F13_BOOT_PROJECT' }
        'F13App' { 'FIRMWARE_F13_APP_PROJECT' }
        'Web' { 'FIRMWARE_WEB_ROOT' }
    }
    $configured = [Environment]::GetEnvironmentVariable($variable)
    if (-not [string]::IsNullOrWhiteSpace($configured)) {
        return [IO.Path]::GetFullPath($configured)
    }
    $parent = Split-Path -Parent ([IO.Path]::GetFullPath($ToolRoot))
    if ($Kind -eq 'Web') { return $parent }
    $name = if ($Kind -eq 'F13Boot') { 'RL78_F13_Boot_loader_CAN' } else { 'RL78_F13_Boot_loader_CAN_app' }
    return Join-Path $parent $name
}
