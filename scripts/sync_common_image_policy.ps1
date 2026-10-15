param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if (!$WebRoot) { $WebRoot=Join-Path (Split-Path -Parent (Split-Path -Parent $repo)) '_Avery_Markdown\web' }
$pattern='(?s)<!-- COMMON_IMAGE_POLICY_BEGIN -->.*?<!-- COMMON_IMAGE_POLICY_END -->'
$source=Get-Content (Join-Path $repo 'docs\COMMON_IMAGE_METADATA.md') -Raw -Encoding UTF8
$block=[regex]::Match($source,$pattern).Value
if (!$block) { throw 'Missing canonical common-policy block' }
$targets=@((Join-Path $repo 'docs\F13_BOOT_UPDATE_FLOW_zh-TW.md'),
    (Join-Path $repo 'docs\SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md'),
    (Join-Path $repo 'docs\MULTIBUS_TOOL_GUIDE_zh-TW.md'),
    (Join-Path $WebRoot 'slide_isp_tool_for_RL78\index.md'),
    (Join-Path $WebRoot 'slide_boot_loader_for_RL78\index.md'))
foreach ($path in $targets) {
    $text=Get-Content -LiteralPath $path -Raw -Encoding UTF8
    $matches=[regex]::Matches($text,$pattern)
    if ($matches.Count -gt 1) { throw "Duplicate policy blocks: $path" }
    if ($CheckOnly) {
        if ($matches.Count -ne 1 -or $matches[0].Value.Replace("`r`n","`n") -cne $block.Replace("`r`n","`n")) { throw "Policy drift: $path" }
        Write-Host "PASS: common policy matches $path"
    } else {
        if ($matches.Count) { $text=[regex]::Replace($text,$pattern,[System.Text.RegularExpressions.MatchEvaluator]{param($m) $block}) }
        else { $text=$text.TrimEnd()+"`r`n`r`n"+$block+"`r`n" }
        [IO.File]::WriteAllText($path,$text,(New-Object Text.UTF8Encoding($false)))
    }
}
