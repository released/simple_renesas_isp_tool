param([Parameter(Mandatory=$true)][string]$DeviceRoot,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path
$common=Join-Path $tool 'firmware\common\isp_device'
function Sync-File([string]$Source,[string]$Destination){
    if(!(Test-Path -LiteralPath (Split-Path -Parent $Destination) -PathType Container)){throw 'Missing existing ISP destination.'}
    if((Test-Path -LiteralPath $Destination) -and (Get-FileHash -LiteralPath $Source).Hash -ceq (Get-FileHash -LiteralPath $Destination).Hash){return}
    if($CheckOnly){throw ('ISP mirror drift: '+$Destination)}
    Copy-Item -LiteralPath $Source -Destination $Destination
}
foreach($mcu in @('F13','F24','G16','G22')){foreach($bus in @('UART','I2C','Xmodem')){
    $boot=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_'+$bus)
    $dest=Join-Path $boot 'isp'
    $names=@('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h',
        'isp_rl78_board.h','isp_bus_contract.h','isp_uart_timing.h')
    if($bus -ne 'UART'){$names+='isp_runtime_transport.inc'}
    if($bus -eq 'Xmodem'){$names+=@('isp_xmodem.c','isp_xmodem.h')}
    if($bus -eq 'I2C'){$names+=@('isp_i2c.c','isp_i2c.h','isp_i2c_mailbox.c','isp_i2c_mailbox.h')}
    foreach($name in $names){Sync-File (Join-Path $common $name) (Join-Path $dest $name)}
    if($mcu -in @('F13','F24')){Sync-File (Join-Path $common 'isp_rl78_board.c') (Join-Path $dest 'isp_rl78_board.c')}
    else{Sync-File (Join-Path $tool 'firmware\common\isp_small\isp_board.c') (Join-Path $dest 'isp_board.c')}
    if($mcu -in @('F24','G16','G22')){
        Sync-File (Join-Path $tool 'firmware\common\rl78_serial_scfg\validate_scfg.ps1') (Join-Path $boot 'validate_scfg.ps1')
    }
    if($mcu -eq 'G16'){Sync-File (Join-Path $tool 'firmware\common\isp_small\g16_flash.c') (Join-Path $boot 'device_flash.c')}
}}
foreach($bus in @('UART','I2C','Xmodem')){
    $dest=Join-Path $DeviceRoot ('Sample_Project_RH850_S1_ISP_Boot_loader_'+$bus+'\common')
    $names=@('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h','isp_bus_contract.h')
    if($bus -eq 'Xmodem'){$names+=@('isp_xmodem.c','isp_xmodem.h')}
    foreach($name in $names){Sync-File (Join-Path $common $name) (Join-Path $dest $name)}
    foreach($name in @('isp_rh850.h','isp_rh850_runtime.c',$(if($bus -eq 'I2C'){'isp_rh850_i2c.c'}else{'isp_rh850_uart.c'}))){
        Sync-File (Join-Path $tool ('firmware\common\isp_rh850\'+$name)) (Join-Path $dest $name)
    }
    Sync-File (Join-Path $tool 'firmware\common\isp_rh850\validate_serial_settings.ps1') (Join-Path $DeviceRoot ('Sample_Project_RH850_S1_ISP_Boot_loader_'+$bus+'\tools\validate_serial_settings.ps1'))
}
Write-Host 'PASS ISP runtime/protocol/board mirrors; existing authoring headers and project files preserved.'
