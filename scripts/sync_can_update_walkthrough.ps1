param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$pattern='(?s)<!-- CAN_WALKTHROUGH_BEGIN -->.*?<!-- CAN_WALKTHROUGH_END -->'
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\CAN_UPDATE_WALKTHROUGH_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if ($blocks.Count -ne 1) { throw 'Expected exactly one canonical CAN walkthrough block.' }
$path=Join-Path $WebRoot 'slide_isp_tool_for_RL78\index.md'
$web=[IO.File]::ReadAllText($path)
$matches=[regex]::Matches($web,$pattern)
if ($matches.Count -ne 1) { throw 'Expected exactly one CAN walkthrough block in ISP web guide.' }
if ($CheckOnly) {
    if ($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")) {
        throw 'CAN update walkthrough differs from canonical MD.'
    }
} else {
    $replacement=$blocks[0].Value
    $updated=[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    [IO.File]::WriteAllText($path,$updated,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: cross-layer CAN update walkthrough synchronized/verified.'
