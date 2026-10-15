param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$pattern='(?s)<!-- PUBLIC_RESOURCES_BEGIN -->.*?<!-- PUBLIC_RESOURCES_END -->'
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\MULTIBUS_TOOL_GUIDE_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if ($blocks.Count -ne 1) { throw 'Expected one canonical public-resources block.' }
$updates=@()
foreach ($relative in @('slide_isp_tool_for_RL78\index.md','slide_boot_loader_for_RL78\index.md')) {
    $path=Join-Path $WebRoot $relative
    $text=[IO.File]::ReadAllText($path)
    $matches=[regex]::Matches($text,$pattern)
    if ($matches.Count -ne 1) { throw "Expected one public-resources block: $path" }
    if ($CheckOnly) {
        if ($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")) {
            throw "Public resources differ: $path"
        }
    } else {
        $replacement=$blocks[0].Value
        $updates+=@{Path=$path;Text=[regex]::Replace($text,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})}
    }
}
foreach ($update in $updates) { [IO.File]::WriteAllText($update.Path,$update.Text,(New-Object Text.UTF8Encoding($false))) }
Write-Host 'PASS: public downloads and six workflow links synchronized/verified.'
