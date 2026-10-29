param([Parameter(Mandatory=$true)][string]$DeviceRoot,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path
$common=Join-Path $tool 'firmware\common\isp_device'
function Sync-File([string]$Source,[string]$Destination){
    if(!(Test-Path -LiteralPath (Split-Path -Parent $Destination) -PathType Container)){throw 'Missing existing ISP destination.'}
    if((Test-Path -LiteralPath $Destination) -and [IO.File]::ReadAllText($Source).Replace("`r`n","`n") -ceq [IO.File]::ReadAllText($Destination).Replace("`r`n","`n")){return}
    if($CheckOnly){throw ('ISP mirror drift: '+$Destination)}
    Copy-Item -LiteralPath $Source -Destination $Destination
}
foreach($mcu in @('F13','F24','G16','G22')){foreach($bus in @('UART','I2C','Xmodem')){
    $boot=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_'+$bus)
    $transfer=Join-Path $DeviceRoot ('RL78_'+$mcu+'_'+$(if($bus -eq 'I2C'){'I2C_Slave'}else{$bus})+'_Transfer')
    foreach($project in @($boot,($boot+'_app'),$transfer)){
    if(!(Test-Path -LiteralPath $project)){continue}
    $names=@('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h',
        'isp_rl78_board.h','isp_bus_contract.h','isp_uart_timing.h')
    # Project workflow is owned by custom_func.c and is not overwritten by API sync.
    if($bus -eq 'Xmodem'){$names+=@('isp_xmodem.c','isp_xmodem.h')}
    if($bus -eq 'I2C'){$names+=@('isp_i2c.c','isp_i2c.h','isp_i2c_mailbox.c','isp_i2c_mailbox.h')}
    foreach($name in $names){Sync-File (Join-Path $common $name) (Get-DeviceProjectSource $project $name)}
    if($mcu -in @('F13','F24')){Sync-File (Join-Path $common 'isp_rl78_board.c') (Get-DeviceProjectSource $project 'isp_rl78_board.c')}
    else{Sync-File (Join-Path $tool 'firmware\common\isp_small\isp_board.c') (Get-DeviceProjectSource $project 'isp_board.c')}
    if($mcu -in @('F24','G16','G22')){
        Sync-File (Join-Path $tool 'firmware\common\rl78_serial_scfg\validate_scfg.ps1') (Join-Path $project 'validate_scfg.ps1')
    }
    if($mcu -eq 'G16'){Sync-File (Join-Path $tool 'firmware\common\isp_small\g16_flash.c') (Get-DeviceProjectSource $project 'device_flash.c')}
}}}
foreach($bus in @('UART','I2C','Xmodem')){
    $boot=Join-Path $DeviceRoot ('Sample_Project_RH850_S1_ISP_Boot_loader_'+$bus)
    $transfer=Join-Path $DeviceRoot ('Sample_Project_RH850_S1_'+$(if($bus -eq 'I2C'){'I2C_Slave'}else{$bus})+'_Transfer')
    foreach($project in @($boot,($boot+'_app'),$transfer)){
    if(!(Test-Path -LiteralPath $project)){continue}
    $names=@('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h','isp_bus_contract.h')
    if($bus -eq 'Xmodem'){$names+=@('isp_xmodem.c','isp_xmodem.h')}
    foreach($name in $names){Sync-File (Join-Path $common $name) (Get-DeviceProjectSource $project $name)}
    foreach($name in @('isp_rh850.h',$(if($bus -eq 'I2C'){'isp_rh850_i2c.c'}else{'isp_rh850_uart.c'}))){
        Sync-File (Join-Path $tool ('firmware\common\isp_rh850\'+$name)) (Get-DeviceProjectSource $project $name)
    }
    foreach($name in @('retarget.c','retarget.h','retarget_full.inc')){
        Sync-File (Join-Path $tool ('firmware\common\isp_rh850\'+$name)) (Join-Path $project $name)
    }
    # The local mode selector is editable and must not be overwritten by API sync.
    if(!(Test-Path -LiteralPath (Join-Path $project 'retarget_config.h'))){throw 'Missing local formatter mode selector.'}
    Sync-File (Join-Path $tool 'firmware\common\isp_rh850\validate_serial_settings.ps1') (Join-Path $project 'tools\validate_serial_settings.ps1')
}
}
foreach($projectName in @('Sample_Project_RH850_S1_Boot_loader_CAN',
    'Sample_Project_RH850_S1_Boot_loader_CAN_app','Sample_Project_RH850_S1_CAN_Transfer',
    'Sample_Project_RH850_S1_Boot_loader_CAN_FD','Sample_Project_RH850_S1_Boot_loader_CAN_FD_app',
    'Sample_Project_RH850_S1_CAN_FD_Transfer','Sample_Project_RH850_S1_CAN_FD_MultiCHv4')){
    $project=Join-Path $DeviceRoot $projectName
    if(!(Test-Path -LiteralPath $project)){continue}
    foreach($name in @('retarget.c','retarget.h','retarget_full.inc')){
        Sync-File (Join-Path $tool ('firmware\common\isp_rh850\'+$name)) (Join-Path $project $name)
    }
    if(!(Test-Path -LiteralPath (Join-Path $project 'retarget_config.h'))){throw 'Missing local formatter mode selector.'}
}
Write-Host 'PASS project-local ISP protocol/board mirrors; custom_func workflows and editable contracts preserved.'
