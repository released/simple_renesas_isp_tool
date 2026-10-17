param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_projection.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
Sync-WebGuideSection -Section UPDATE_EVIDENCE -SourcePath (Join-Path $repo 'docs\UPDATE_VALIDATION_zh-TW.md') -WebRoot $WebRoot -CheckOnly:$CheckOnly
