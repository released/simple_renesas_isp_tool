param([Parameter(Mandatory=$true)][ValidateSet('boot','app','transfer')][string]$Kind,
      [Parameter(Mandatory=$true)][string]$Project)
$ErrorActionPreference='Stop'
$Project=(Resolve-Path -LiteralPath $Project).Path
$name=Split-Path -Leaf $PSScriptRoot
if($name -notmatch '^RL78_(G16|G22)_ISP_Boot_loader_(UART|I2C|Xmodem)$'){throw 'Unexpected Boot contract folder.'}
$ram=if($Matches[1] -eq 'G16'){0xFF700}else{0xFEF00}
$projectName=Split-Path -Leaf $Project
$map=Join-Path $Project ('DefaultBuild\'+$projectName+'.map')
if($Kind -eq 'app') {
    Push-Location -LiteralPath $Project
    try { & (Join-Path $Project 'generate_update_image.ps1') } finally {Pop-Location}
}else { & (Join-Path $PSScriptRoot 'verify_boot_budget.ps1') -MapPath $map }
& (Join-Path $PSScriptRoot 'verify_ram_budget.ps1') -MapPath $map -RamStart $ram -Owner ($ram+16)
Write-Host 'PASS package/ROM/RAM gates; a failed earlier gate cannot be hidden by a later command.'

# APP_TIMING_BINS_BEGIN
if($Kind -eq 'app') {
    $LASTEXITCODE=0
    & (Join-Path $Project 'generate_test_bins.ps1') -ProjectRoot $Project -Automatic
    if($LASTEXITCODE -ne 0){throw 'App timing BIN generation failed.'}
}
# APP_TIMING_BINS_END
