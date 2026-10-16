param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_projection.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$docs=@('F13_BOOT_UPDATE_FLOW_zh-TW.md','SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md','MULTIBUS_TOOL_GUIDE_zh-TW.md') |
    ForEach-Object {Join-Path $repo ('docs\'+$_)}
Sync-WebGuideSection -Section COMMON_IMAGE_POLICY -SourcePath (Join-Path $repo 'docs\COMMON_IMAGE_METADATA.md') `
    -WebRoot $WebRoot -CheckOnly:$CheckOnly -DocTargets $docs
