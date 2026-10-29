[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [ValidateSet('All','F13')][string]$Scope='All'
)
$ErrorActionPreference='Stop'
# Preserve the original three-CAN-App/F13 scopes through the bounded common runner.
$names=@('RL78_F13_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_FD_app')
if($Scope -eq 'F13'){$names=@($names[0])}
& (Join-Path $PSScriptRoot 'test_app_timing_bins.ps1') -DeviceRoot $SourceRoot -ProjectName $names -Native
