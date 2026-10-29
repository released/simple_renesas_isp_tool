param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_projection.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$begin='<!-- F24_APP_IRQ_BEGIN -->'; $end='<!-- F24_APP_IRQ_END -->'
$pattern='(?s)'+[regex]::Escape($begin)+'.*?'+[regex]::Escape($end)
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if ($blocks.Count -ne 1) { throw 'Expected one canonical F24 app IRQ block.' }
$updates=@()
foreach ($relative in @('slide_simple_renesas_isp_tool\index.md','slide_boot_loader_for_RL78\index.md')) {
    $expected=Get-WebGuideProjection -Section F24_APP_IRQ -Folder ($relative.Split('\')[0]) -Canonical $blocks[0].Value
    $path=Get-WebGuideSectionPath -WebRoot $WebRoot -Folder ($relative.Split('\')[0]) -Section F24_APP_IRQ
    $text=[IO.File]::ReadAllText($path)
    $matches=[regex]::Matches($text,$pattern)
    if ($matches.Count -ne 1) { throw "Expected one F24 managed block: $path" }
    if ($CheckOnly) {
        if ($matches[0].Value.Replace("`r`n","`n") -cne $expected.Replace("`r`n","`n")) { throw "F24 app IRQ guide differs: $path" }
    } else {
        $replacement=$expected
        $updates+=@{Path=$path;Text=[regex]::Replace($text,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})}
    }
}
foreach ($update in $updates) { [IO.File]::WriteAllText($update.Path,$update.Text,(New-Object Text.UTF8Encoding($false))) }
Write-Host 'PASS: host App engineering owner section and jump link synchronized/verified.'
