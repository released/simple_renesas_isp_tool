param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_projection.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$pattern='(?s)<!-- F24_DEVICE_BEGIN -->.*?<!-- F24_DEVICE_END -->'
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\F24_CAN_DEVICE_DESIGN_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if($blocks.Count -ne 1){throw 'Expected exactly one canonical F24 device block.'}
$updates=@()
foreach($folder in @('slide_isp_tool_for_RL78','slide_boot_loader_for_RL78')){
    $expected=Get-WebGuideProjection -Section F24_DEVICE -Folder $folder -Canonical $blocks[0].Value
    $path=Join-Path $WebRoot ($folder+'\index.md')
    $web=[IO.File]::ReadAllText($path)
    $matches=[regex]::Matches($web,$pattern)
    if($matches.Count -ne 1){throw "Expected exactly one F24 device block: $folder"}
    if($CheckOnly){
        if($matches[0].Value.Replace("`r`n","`n") -cne $expected.Replace("`r`n","`n")){throw "F24 device design differs: $folder"}
    }else{
        $replacement=$expected
        $updated=[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
        $updates+=@{Path=$path;Text=$updated}
    }
}
foreach($update in $updates){[IO.File]::WriteAllText($update.Path,$update.Text,(New-Object Text.UTF8Encoding($false)))}
Write-Host 'PASS: F24 CAN device design synchronized/verified in both web guides.'
