param([string]$WebRoot='',[switch]$CheckOnly)
$ErrorActionPreference='Stop';$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_projection.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\RH850_CAN_DEVICE_GUIDE_zh-TW.md'))
$pattern='(?s)<!-- RH850_DEVICE_BEGIN -->.*?<!-- RH850_DEVICE_END -->'
$block=[regex]::Matches($source,$pattern);if($block.Count -ne 1){throw 'Expected one canonical RH850 block.'}
$pending=@()
foreach($folder in @('slide_boot_loader_for_RL78','slide_isp_tool_for_RL78')){
    $path=Get-WebGuideSectionPath -WebRoot $WebRoot -Folder $folder -Section RH850_DEVICE
    $web=[IO.File]::ReadAllText($path)
    $matches=[regex]::Matches($web,$pattern)
    if($matches.Count -gt 1){throw 'Duplicate RH850 projection.'}
    $replacement=Get-WebGuideProjection -Section RH850_DEVICE -Folder $folder -Canonical $block[0].Value
    if($CheckOnly){if($matches.Count -ne 1 -or $matches[0].Value.Replace("`r`n","`n") -cne $replacement.Replace("`r`n","`n")){throw 'RH850 web projection differs.'}}
    else{
        $updated=if($matches.Count){[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})}else{$web+"`r`n`r`n"+$replacement+"`r`n"}
        $pending+=@{Path=$path;Text=$updated}
    }
}
foreach($item in $pending){[IO.File]::WriteAllText($item.Path,$item.Text,(New-Object Text.UTF8Encoding($false)))}
Write-Host 'PASS: RH850 canonical MD / Bootloader web projection.'
