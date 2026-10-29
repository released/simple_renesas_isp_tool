param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_layout.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\MULTIBUS_TOOL_GUIDE_zh-TW.md'))
$path=Get-WebGuideSectionPath -WebRoot $WebRoot -Folder slide_simple_renesas_isp_tool -Section TRANSFER_EXAMPLES
$web=[IO.File]::ReadAllText($path)
$updated=$web
# Validate both canonical sections before writing the page once.
foreach($section in @('TRANSFER_EXAMPLES','TRANSFER_DIAGNOSTICS')){
    $pattern='(?s)<!-- '+$section+'_BEGIN -->.*?<!-- '+$section+'_END -->'
    $blocks=[regex]::Matches($source,$pattern)
    if($blocks.Count -ne 1){throw "Expected one canonical $section block."}
    $matches=[regex]::Matches($web,$pattern)
    if($matches.Count -ne 1){throw "Expected one $section block in ISP guide."}
    if($CheckOnly){
        if($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")){
            throw "$section guide differs."
        }
    }else{
        $replacement=$blocks[0].Value
        $updated=[regex]::Replace($updated,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    }
}
if(!$CheckOnly){
    [IO.File]::WriteAllText($path,$updated,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: Transfer example categories and diagnostics MD/ISP sections synchronized/verified.'
