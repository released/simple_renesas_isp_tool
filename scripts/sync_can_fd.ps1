param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_projection.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
foreach($section in @('FD_TRANSFER','FD_UPDATE','FD_DEVICE')){
    Sync-WebGuideSection -Section $section -SourcePath (Join-Path $repo 'docs\CAN_FD_UPDATE_zh-TW.md') -WebRoot $WebRoot -CheckOnly:$CheckOnly
}
