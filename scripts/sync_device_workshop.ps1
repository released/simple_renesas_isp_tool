param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$pattern='(?s)<!-- DEVICE_WORKSHOP_BEGIN -->.*?<!-- DEVICE_WORKSHOP_END -->'
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if($blocks.Count -ne 1){throw 'Expected one canonical device workshop block.'}
$updates=@()
# Validate both targets before writing either. Never insert a missing block.
foreach($folder in @('slide_boot_loader_for_RL78','slide_isp_tool_for_RL78')){
    $path=Join-Path $WebRoot ($folder+'\index.md')
    $web=[IO.File]::ReadAllText($path)
    $matches=[regex]::Matches($web,$pattern)
    if($matches.Count -ne 1){throw "Expected one device workshop block: $folder"}
    if($CheckOnly){
        if($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")){
            throw "Device workshop differs: $folder"
        }
    }else{
        $replacement=$blocks[0].Value
        $updated=[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
        $updates+=@([pscustomobject]@{Path=$path;Text=$updated})
    }
}
foreach($update in $updates){
    [IO.File]::WriteAllText($update.Path,$update.Text,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: F13/F24 device Metadata/vector workshop synchronized/verified.'
