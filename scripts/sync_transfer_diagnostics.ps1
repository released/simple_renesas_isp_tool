param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$pattern='(?s)<!-- TRANSFER_DIAGNOSTICS_BEGIN -->.*?<!-- TRANSFER_DIAGNOSTICS_END -->'
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\MULTIBUS_TOOL_GUIDE_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if($blocks.Count -ne 1){throw 'Expected one canonical Transfer diagnostics block.'}
$path=Join-Path $WebRoot 'slide_isp_tool_for_RL78\index.md'
$web=[IO.File]::ReadAllText($path)
$matches=[regex]::Matches($web,$pattern)
if($matches.Count -ne 1){throw 'Expected one Transfer diagnostics block in ISP guide.'}
if($CheckOnly){
    if($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")){
        throw 'Transfer diagnostics guide differs.'
    }
}else{
    $replacement=$blocks[0].Value
    $updated=[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    [IO.File]::WriteAllText($path,$updated,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: Transfer diagnostics MD/ISP section synchronized/verified.'
