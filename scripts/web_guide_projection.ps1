# Shared single-owner web projection. Non-owner pages contain links, not copies.
function Get-WebGuideProjection {
    param([string]$Section, [string]$Folder, [string]$Canonical)
    $catalog = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'web_guide_sections.json')) | ConvertFrom-Json
    $entry = $catalog.PSObject.Properties[$Section]
    if (!$entry) { throw "Unknown guide section: $Section" }
    $config = $entry.Value
    if ($Folder -ceq $config.owner) { return $Canonical }
    $url = 'https://released.github.io/'+$config.owner+'/#'+$config.anchor
    return "<!-- ${Section}_BEGIN -->`n<a id=`"$($config.anchor)`"></a>`n`n## $($config.title)`n`n$($config.description)`n[閱讀完整章節]($url)`n<!-- ${Section}_END -->"
}

function Sync-WebGuideSection {
    param([string]$Section, [string]$SourcePath, [string]$WebRoot,
          [switch]$CheckOnly, [string[]]$DocTargets = @())
    $pattern = '(?s)<!-- '+$Section+'_BEGIN -->.*?<!-- '+$Section+'_END -->'
    $source = [IO.File]::ReadAllText($SourcePath)
    $blocks = [regex]::Matches($source,$pattern)
    if ($blocks.Count -ne 1) { throw "Expected one canonical section: $Section" }
    $canonical = $blocks[0].Value
    $targets = @()
    foreach ($folder in @('slide_boot_loader_for_RL78','slide_isp_tool_for_RL78')) {
        $targets += @{Path=(Join-Path $WebRoot ($folder+'\index.md'));Folder=$folder}
    }
    foreach ($path in $DocTargets) { $targets += @{Path=$path;Folder='docs'} }
    $updates = @()
    # Validate all marker counts and expected projections before writing any file.
    foreach ($target in $targets) {
        $text = [IO.File]::ReadAllText($target.Path)
        $matches = [regex]::Matches($text,$pattern)
        if ($matches.Count -ne 1) { throw "Expected one $Section block: $($target.Path)" }
        $expected = Get-WebGuideProjection -Section $Section -Folder $target.Folder -Canonical $canonical
        if ($CheckOnly) {
            if ($matches[0].Value.Replace("`r`n","`n") -cne $expected.Replace("`r`n","`n")) {
                throw "Guide projection differs: $($target.Path) / $Section"
            }
        } else {
            $replacement = $expected
            $updates += @{Path=$target.Path;Text=[regex]::Replace($text,$pattern,
                [Text.RegularExpressions.MatchEvaluator]{param($m) $replacement})}
        }
    }
    foreach ($update in $updates) {
        [IO.File]::WriteAllText($update.Path,$update.Text,(New-Object Text.UTF8Encoding($false)))
    }
    Write-Host "PASS: single-owner guide section $Section synchronized/verified."
}
