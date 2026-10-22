# Companion repositories default to siblings; overrides are machine-local.
function Get-CompanionPath {
    param(
        [ValidateSet('F13Transfer','F13Boot','F13App','F24Transfer','F24Boot','F24App',
                     'F24FdTransfer','F24FdBoot','F24FdApp','Web')][string]$Kind,
        [string]$ExplicitPath,
        [string]$ToolRoot = (Split-Path -Parent $PSScriptRoot)
    )
    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return [IO.Path]::GetFullPath($ExplicitPath)
    }
    $variable = switch ($Kind) {
        'F13Transfer' { 'FIRMWARE_F13_DEVICE_TRANSFER_PROJECT' }
        'F13Boot' { 'FIRMWARE_F13_BOOT_PROJECT' }
        'F13App' { 'FIRMWARE_F13_APP_PROJECT' }
        'F24Transfer' { 'FIRMWARE_F24_DEVICE_TRANSFER_PROJECT' }
        'F24Boot' { 'FIRMWARE_F24_DEVICE_BOOT_PROJECT' }
        'F24App' { 'FIRMWARE_F24_DEVICE_APP_PROJECT' }
        'F24FdTransfer' { 'FIRMWARE_F24_DEVICE_FD_TRANSFER_PROJECT' }
        'F24FdBoot' { 'FIRMWARE_F24_DEVICE_FD_BOOT_PROJECT' }
        'F24FdApp' { 'FIRMWARE_F24_DEVICE_FD_APP_PROJECT' }
        'Web' { 'FIRMWARE_WEB_ROOT' }
    }
    $configured = [Environment]::GetEnvironmentVariable($variable)
    if (-not [string]::IsNullOrWhiteSpace($configured)) {
        return [IO.Path]::GetFullPath($configured)
    }
    $parent = Split-Path -Parent ([IO.Path]::GetFullPath($ToolRoot))
    if ($Kind -eq 'Web') { return $parent }
    $name = switch ($Kind) {
        'F13Transfer' { 'RL78_F13_CAN_Transfer' }
        'F13Boot' { 'RL78_F13_Boot_loader_CAN' }
        'F13App' { 'RL78_F13_Boot_loader_CAN_app' }
        'F24Transfer' { 'RL78_F24_CAN_Transfer' }
        'F24Boot' { 'RL78_F24_Boot_loader_CAN' }
        'F24App' { 'RL78_F24_Boot_loader_CAN_app' }
        'F24FdTransfer' { 'RL78_F24_CAN_FD_Transfer' }
        'F24FdBoot' { 'RL78_F24_Boot_loader_CAN_FD' }
        'F24FdApp' { 'RL78_F24_Boot_loader_CAN_FD_app' }
    }
    return Join-Path $parent $name
}
