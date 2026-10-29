param([Parameter(Mandatory=$true)][string]$DeviceRoot,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'device_app_catalog.ps1')
. (Join-Path $PSScriptRoot 'device_test_bins_output.ps1')
# Use the same plain-path guard as the generator without executing generation.
function Assert-TestBinPlainPath([string]$Path) {
    $cursor=[IO.Path]::GetFullPath($Path)
    while($cursor) {
        if((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Reparse integration target refused.'
        }
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
}
$apps=@(Get-DeviceTimingApps $DeviceRoot)
$changes=@();$utf8=New-Object Text.UTF8Encoding($false)
$appHook=@'
# APP_TIMING_BINS_BEGIN
$LASTEXITCODE=0
& (Join-Path $PSScriptRoot 'generate_test_bins.ps1') -ProjectRoot $PSScriptRoot -Automatic
if($LASTEXITCODE -ne 0){throw 'App timing BIN generation failed.'}
# APP_TIMING_BINS_END
'@
$rlHook=@'
# APP_TIMING_BINS_BEGIN
if($Kind -eq 'app') {
    $LASTEXITCODE=0
    & (Join-Path $Project 'generate_test_bins.ps1') -ProjectRoot $Project -Automatic
    if($LASTEXITCODE -ne 0){throw 'App timing BIN generation failed.'}
}
# APP_TIMING_BINS_END
'@
$rhHook=@'
# APP_TIMING_BINS_BEGIN
if($isApp) {
    $LASTEXITCODE=0
    & (Join-Path $Project 'generate_test_bins.ps1') -ProjectRoot $Project -Automatic
    if($LASTEXITCODE -ne 0){throw 'App timing BIN generation failed.'}
}
# APP_TIMING_BINS_END
'@
$readme=@'
## App timing test images

A successful App build also produces FF-filled test BINs under `test_bins/auto/`.
Open `test_bins/auto/index.html` to select an image by actual size, CRC and hash.
The set includes supported Flash-budget tiers and the declared App capacity;
tiers smaller than the executable or larger than physical Flash are skipped.
The packaged `DefaultBuild/*_update.bin`, code, IRQ tables and release settings
remain unchanged. Only test-image length, FF padding and final CRC are updated.
Automatic rebuilds refresh this one owned set; changed or unknown files stop
replacement. Manual `generate_test_bins.bat` creates a separate batch.
Use the matching ISP workflow and keep its JSON/HTML operation report for timing.
See the [test-image guide](https://released.github.io/slide_boot_loader_for_RL78/#device-test-images).
'@
foreach($app in $apps) {
    Assert-TestBinPlainPath $app.Project
    if($app.Name -match '^RL78_F13_') {$path=Join-Path $app.Project 'generate_update_image.ps1';$hook=$appHook}
    elseif($app.Name -match '^RL78_(G16|G22)_') {$path=Join-Path $app.Project 'post_build.ps1';$hook=$rlHook}
    elseif($app.Name -match '^RL78_F24_') {$path=Join-Path $app.Project 'tools\package_images.ps1';$hook=$rlHook}
    else {$path=Join-Path $app.Project 'tools\device_build.ps1';$hook=$rhHook}
    Assert-TestBinPlainPath $path
    $text=[IO.File]::ReadAllText($path)
    $newline=if($text.Contains("`r`n")){"`r`n"}else{"`n"}
    $wanted=$hook.Replace("`r`n","`n").Replace("`n",$newline)
    if($text.Contains('# APP_TIMING_BINS_BEGIN')) {
        if([regex]::Matches($text,'# APP_TIMING_BINS_BEGIN').Count -ne 1){throw 'Duplicate timing hook.'}
        $current=[regex]::Match($text,'(?s)# APP_TIMING_BINS_BEGIN.*?# APP_TIMING_BINS_END').Value
        if($current.Replace("`r`n","`n").TrimEnd() -cne $hook.Replace("`r`n","`n").TrimEnd()){throw ('Unexpected edited timing hook: '+$path)}
    } else {$changes+=[pscustomobject]@{Path=$path;Text=$text.TrimEnd()+$newline+$newline+$wanted+$newline}}
    $ignore=Join-Path $app.Project '.gitignore'
    $body=if(Test-Path -LiteralPath $ignore){[IO.File]::ReadAllText($ignore)}else{''}
    if($body -notmatch '(?m)^/?test_bins/\s*$'){$changes+=[pscustomobject]@{Path=$ignore;Text=$body.TrimEnd()+"`n/test_bins/`n"}}
    $path=Join-Path $app.Project 'README.md';$text=[IO.File]::ReadAllText($path)
    if(!$text.Contains('## App timing test images')) {
        $text=[regex]::Replace($text,'(?ms)^## FF-filled App test images\r?\n.*?(?=^## |\z)','')
        $changes+=[pscustomobject]@{Path=$path;Text=$text.TrimEnd()+"`n`n"+$readme+"`n"}
    }
}
if($CheckOnly) {
    if($changes.Count){throw ('App timing integration is incomplete: '+(($changes|ForEach-Object {$_.Path}) -join ', '))}
} else {
    foreach($change in $changes){Assert-TestBinPlainPath $change.Path;[IO.File]::WriteAllText($change.Path,$change.Text,$utf8)}
}
& (Join-Path $PSScriptRoot 'sync_device_test_bins.ps1') -DeviceRoot $DeviceRoot -CheckOnly:$CheckOnly
Write-Host ('PASS post-package timing hooks / ignored outputs / reader instructions: '+$apps.Count+' Apps.')
