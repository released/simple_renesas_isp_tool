param([Parameter(Mandatory=$true)][string]$BootProject,[ValidateSet('UART','I2C','Xmodem')][string]$Bus)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
$boot=(Resolve-Path -LiteralPath $BootProject).Path
$prefix=Split-Path -Parent $boot
$targets=@($boot,($boot+'_app'),(Join-Path $prefix ('Sample_Project_RH850_S1_'+$(if($Bus -eq 'I2C'){'I2C_Slave'}else{$Bus})+'_Transfer')))
foreach($project in $targets){
    if(!(Test-Path -LiteralPath $project)){continue}
    Assert-DeviceLocalBindings $project
    if(!(Test-Path -LiteralPath (Join-Path $project 'tools\device_build.ps1'))){throw 'Missing local build tools; initialize from an independent golden sample.'}
}
Write-Host 'PASS local RH850 serial tools/source bindings; user clock, SC and linker settings preserved.'
