param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_layout.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$pattern='(?s)<!-- FLASH_REFERENCE_BEGIN -->.*?<!-- FLASH_REFERENCE_END -->'
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if ($blocks.Count -ne 1) { throw 'Expected exactly one canonical flash reference block.' }
$path=Get-WebGuideSectionPath -WebRoot $WebRoot -Folder slide_boot_loader_for_RL78 -Section FLASH_REFERENCE
$web=[IO.File]::ReadAllText($path)
$matches=[regex]::Matches($web,$pattern)
if ($matches.Count -ne 1) { throw 'Expected exactly one flash reference block in boot web guide.' }
if ($CheckOnly) {
    if ($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")) {
        throw 'Flash/metadata/CAN byte reference differs from canonical MD.'
    }
} else {
    $replacement=$blocks[0].Value
    $updated=[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    [IO.File]::WriteAllText($path,$updated,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: complete Flash/metadata/CAN byte reference synchronized/verified.'
