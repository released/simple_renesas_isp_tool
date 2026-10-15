param([string]$WebRoot = '', [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$toolRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$begin = '<!-- F13_BOOT_DECISION_BEGIN -->'
$end = '<!-- F13_BOOT_DECISION_END -->'
$pattern = '(?s)' + [regex]::Escape($begin) + '.*?' + [regex]::Escape($end)
$source = Get-Content -LiteralPath (Join-Path $toolRoot 'docs\F13_BOOT_UPDATE_FLOW_zh-TW.md') -Raw -Encoding UTF8
$blocks = [regex]::Matches($source, $pattern)
if ($blocks.Count -ne 1) { throw 'Expected exactly one canonical boot-decision block.' }
$targets = @('slide_isp_tool_for_RL78\index.md', 'slide_boot_loader_for_RL78\index.md')
# Validate every target before changing either local web draft.
$updates = @()
foreach ($relative in $targets) {
    $path = Join-Path $WebRoot $relative
    $text = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    if ([regex]::Matches($text, $pattern).Count -ne 1) { throw "Expected one managed block in $path" }
    if ($CheckOnly) {
        $actual = [regex]::Match($text, $pattern).Value.Replace("`r`n", "`n")
        if ($actual -cne $blocks[0].Value.Replace("`r`n", "`n")) { throw "Boot-decision text differs: $path" }
        Write-Host "PASS: boot-decision text matches canonical MD -> $path"
        continue
    }
    $replacement = $blocks[0].Value
    $updated = [regex]::Replace($text, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $replacement })
    $updates += [pscustomobject]@{ Path = $path; Text = $updated }
}
foreach ($update in $updates) {
    [System.IO.File]::WriteAllText($update.Path, $update.Text, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "PASS: synchronized full F13 boot decision -> $($update.Path)"
}
