param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if (!$WebRoot) { $WebRoot=Join-Path (Split-Path -Parent (Split-Path -Parent $repo)) '_Avery_Markdown\web' }
$begin='<!-- RL78_PORTING_BEGIN -->'; $end='<!-- RL78_PORTING_END -->'
$pattern='(?s)'+[regex]::Escape($begin)+'.*?'+[regex]::Escape($end)
$source=[IO.File]::ReadAllText((Join-Path $repo 'docs\RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md'))
$blocks=[regex]::Matches($source,$pattern)
if ($blocks.Count -ne 1) { throw 'Expected one canonical RL78 porting block.' }
$path=Join-Path $WebRoot 'slide_boot_loader_for_RL78\index.md'
$web=[IO.File]::ReadAllText($path)
$matches=[regex]::Matches($web,$pattern)
if ($matches.Count -ne 1) { throw 'Expected one RL78 porting block in boot web guide.' }
if ($CheckOnly) {
    if ($matches[0].Value.Replace("`r`n","`n") -cne $blocks[0].Value.Replace("`r`n","`n")) {
        throw 'RL78 porting guide differs from canonical MD.'
    }
} else {
    $replacement=$blocks[0].Value
    $updated=[regex]::Replace($web,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    [IO.File]::WriteAllText($path,$updated,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: local web RL78 Boot/App porting guide synchronized/verified.'
