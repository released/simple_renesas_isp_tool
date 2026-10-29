param([Parameter(Mandatory=$true)][string]$DeviceRoot,
    [ValidateSet('UART','I2C','Xmodem')][string[]]$Bus=@('UART','I2C','Xmodem'),[string]$ReferenceRoot='')
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'initialize_device_samples.ps1') -DeviceRoot $DeviceRoot -ReferenceRoot $ReferenceRoot -Mcu RH850 -Bus $Bus
