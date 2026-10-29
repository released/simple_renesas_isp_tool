param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_projection.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
Sync-WebGuideSection -Section UART_TOOL_GUIDE -SourcePath (Join-Path $repo 'docs\ISP_UART_TOOL_GUIDE_zh-TW.md') -WebRoot $WebRoot -CheckOnly:$CheckOnly
Sync-WebGuideSection -Section SERIAL_DEVICE_GUIDE -SourcePath (Join-Path $repo 'docs\ISP_UART_DEVICE_DESIGN_zh-TW.md') -WebRoot $WebRoot -CheckOnly:$CheckOnly
# Refresh the paired chapter navigation and transport pages from the same source.
& (Join-Path $PSScriptRoot 'sync_rh850_device.ps1') -WebRoot $WebRoot -CheckOnly:$CheckOnly
