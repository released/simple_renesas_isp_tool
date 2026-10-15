param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$pattern='(?s)<!-- F24_DEVICE_BEGIN -->.*?<!-- F24_DEVICE_END -->'
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\F24_CAN_DEVICE_DESIGN_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if($blocks.Count -ne 1){throw 'Expected exactly one canonical F24 device block.'}
foreach($folder in @('slide_isp_tool_for_RL78','slide_boot_loader_for_RL78')){
    $path=Join-Path $WebRoot ($folder+'\index.md')
    $web=[IO.File]::ReadAllText($path)
    $matches=[regex]::Matches($web,$pattern)
    if($matches.Count -ne 1){throw "Expected exactly one F24 device block: $folder"}
    if($CheckOnly){
        if($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")){throw "F24 device design differs: $folder"}
    }else{
        $replacement=$blocks[0].Value
        $updated=[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
        [IO.File]::WriteAllText($path,$updated,(New-Object Text.UTF8Encoding($false)))
    }
}
Write-Host 'PASS: F24 CAN device design synchronized/verified in both web guides.'
