param([Parameter(Mandatory=$true)][string]$DeviceRoot,
    [string]$G16RfSp='', [string]$G22Rfd='', [string]$G16DeviceHeader='', [string]$G22DeviceHeader='',
    [ValidateSet('G16','G22')][string[]]$Mcu=@('G16','G22'),[string]$ReferenceRoot='')
$ErrorActionPreference='Stop'
$overrides=@{}
if($G16RfSp -or $G16DeviceHeader){$overrides.G16=@{Library=$G16RfSp;Header=$G16DeviceHeader}}
if($G22Rfd -or $G22DeviceHeader){$overrides.G22=@{Library=$G22Rfd;Header=$G22DeviceHeader}}
& (Join-Path $PSScriptRoot 'initialize_device_samples.ps1') -DeviceRoot $DeviceRoot -ReferenceRoot $ReferenceRoot -Mcu $Mcu -Bus UART -VendorOverrides $overrides
