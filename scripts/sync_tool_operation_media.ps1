param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'web_guide_layout.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$sourceRoot=Join-Path $repo 'docs'
$source=[IO.File]::ReadAllText((Join-Path $sourceRoot 'SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md'))
$site=Join-Path $WebRoot 'slide_simple_renesas_isp_tool'
$updates=@{}
$assets=@()
# Validate every block and asset before changing the page or copying assets.
foreach($section in @('TOOL_CONNECT_MEDIA','TOOL_DEVICE_MEDIA','TOOL_BRIDGE_MEDIA')) {
    $page=Get-WebGuideSectionPath -WebRoot $WebRoot -Folder slide_simple_renesas_isp_tool -Section $section
    $text=if($updates.ContainsKey($page)){$updates[$page]}else{[IO.File]::ReadAllText($page)}
    $pattern='(?s)<!-- '+$section+'_BEGIN -->.*?<!-- '+$section+'_END -->'
    $canonical=[regex]::Matches($source,$pattern)
    $current=[regex]::Matches($text,$pattern)
    if($canonical.Count -ne 1 -or $current.Count -ne 1) { throw "Expected one $section block in source and destination." }
    $replacement=$canonical[0].Value
    if($CheckOnly -and $current[0].Value.Replace("`r`n","`n") -cne $replacement.Replace("`r`n","`n")) {
        throw "Operation block differs: $section"
    }
    $text=[regex]::Replace($text,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    $updates[$page]=$text
    foreach($match in [regex]::Matches($replacement,'!\[[^\]]*\]\((images/tool_operation/[A-Za-z0-9_.-]+\.(?:jpg|png))\)')) {
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
if($assets.Count -ne 16 -or @($assets.Source | Select-Object -Unique).Count -ne 16) { throw 'Expected sixteen unique public screenshots.' }
if(!$CheckOnly) {
    foreach($asset in $assets) {
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $asset.Destination) -Force)
        Copy-Item -LiteralPath $asset.Source -Destination $asset.Destination -Force
    }
    foreach($page in $updates.Keys){[IO.File]::WriteAllText($page,$updates[$page],(New-Object Text.UTF8Encoding($false)))}
}
Write-Host 'PASS: three canonical operation blocks and sixteen public screenshots synchronized/verified.'
