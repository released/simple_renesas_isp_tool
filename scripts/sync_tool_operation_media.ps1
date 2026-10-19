param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$sourceRoot=Join-Path $repo 'docs'
$source=[IO.File]::ReadAllText((Join-Path $sourceRoot 'SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md'))
$site=Join-Path $WebRoot 'slide_isp_tool_for_RL78'
$page=Join-Path $site 'index.md'
$text=[IO.File]::ReadAllText($page)
$assets=@()
# Validate every block and asset before changing the page or copying assets.
foreach($section in @('TOOL_CONNECT_MEDIA','TOOL_DEVICE_MEDIA','TOOL_BRIDGE_MEDIA')) {
    $pattern='(?s)<!-- '+$section+'_BEGIN -->.*?<!-- '+$section+'_END -->'
    $canonical=[regex]::Matches($source,$pattern)
    $current=[regex]::Matches($text,$pattern)
    if($canonical.Count -ne 1 -or $current.Count -ne 1) { throw "Expected one $section block in source and destination." }
    $replacement=$canonical[0].Value
    if($CheckOnly -and $current[0].Value.Replace("`r`n","`n") -cne $replacement.Replace("`r`n","`n")) {
        throw "Operation block differs: $section"
    }
    $text=[regex]::Replace($text,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    foreach($match in [regex]::Matches($replacement,'!\[[^\]]*\]\((images/tool_operation/[A-Za-z0-9_.-]+\.jpg)\)')) {
        $relative=$match.Groups[1].Value
        $path=Join-Path $sourceRoot $relative
        if(!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing public screenshot: $path" }
        $destination=Join-Path $site $relative
        $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if($CheckOnly -and (!(Test-Path -LiteralPath $destination -PathType Leaf) -or
                (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -cne $hash)) {
            throw "Screenshot differs or is missing: $relative"
        }
        $assets+=@{Source=$path;Destination=$destination}
    }
}
if($assets.Count -ne 14 -or @($assets.Source | Select-Object -Unique).Count -ne 14) { throw 'Expected fourteen unique public screenshots.' }
if(!$CheckOnly) {
    foreach($asset in $assets) {
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $asset.Destination) -Force)
        Copy-Item -LiteralPath $asset.Source -Destination $asset.Destination -Force
    }
    [IO.File]::WriteAllText($page,$text,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: three canonical operation blocks and fourteen public screenshots synchronized/verified.'
