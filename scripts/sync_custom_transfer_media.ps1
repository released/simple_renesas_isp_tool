param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$sourceRoot=Join-Path $repo 'docs'
$source=[IO.File]::ReadAllText((Join-Path $sourceRoot 'MULTIBUS_TOOL_GUIDE_zh-TW.md'))
$site=Join-Path $WebRoot 'slide_isp_tool_for_RL78'
$page=Join-Path $site 'index.md'
$text=[IO.File]::ReadAllText($page)
$pattern='(?s)<!-- CUSTOM_TRANSFER_MEDIA_BEGIN -->.*?<!-- CUSTOM_TRANSFER_MEDIA_END -->'
$canonical=[regex]::Matches($source,$pattern)
$current=[regex]::Matches($text,$pattern)
if($canonical.Count -ne 1 -or $current.Count -ne 1){throw 'Expected one canonical Custom Transfer media block in source and destination.'}
$replacement=$canonical[0].Value
if($CheckOnly -and $current[0].Value.Replace("`r`n","`n") -cne $replacement.Replace("`r`n","`n")){
    throw 'Custom Transfer media block differs.'
}
$assets=@()
foreach($match in [regex]::Matches($replacement,'!\[[^\]]*\]\((images/custom_transfer/[A-Za-z0-9_.-]+\.png)\)')){
    $relative=$match.Groups[1].Value
    $path=Join-Path $sourceRoot $relative
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing public screenshot: $relative"}
    $destination=Join-Path $site $relative
    if($CheckOnly -and (!(Test-Path -LiteralPath $destination -PathType Leaf) -or
        (Get-FileHash -LiteralPath $path).Hash -cne (Get-FileHash -LiteralPath $destination).Hash)){
        throw "Public screenshot differs: $relative"
    }
    $assets+=@{Source=$path;Destination=$destination}
}
if($assets.Count -ne 4 -or @($assets.Source | Select-Object -Unique).Count -ne 4){throw 'Expected four unique public Custom Transfer screenshots.'}
# Validate all text/assets before writing. Originals are never read or published.
if(!$CheckOnly){
    foreach($asset in $assets){
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $asset.Destination) -Force)
        Copy-Item -LiteralPath $asset.Source -Destination $asset.Destination -Force
    }
    $updated=[regex]::Replace($text,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$replacement})
    [IO.File]::WriteAllText($page,$updated,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: canonical Custom Transfer block and four public screenshots synchronized/verified.'
