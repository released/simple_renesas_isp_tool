param([Parameter(Mandatory=$true)][string]$DeviceRoot,
    [ValidateSet('F13','F24','G16','G22')][string[]]$Mcu=@('F13','F24','G16','G22'),
    [ValidateSet('I2C','Xmodem')][string[]]$Bus=@('I2C','Xmodem'),[string]$ReferenceRoot='')
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'initialize_device_samples.ps1') -DeviceRoot $DeviceRoot -ReferenceRoot $ReferenceRoot -Mcu $Mcu -Bus $Bus
