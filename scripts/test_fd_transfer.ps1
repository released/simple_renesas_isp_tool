param([Parameter(Mandatory=$true)][string]$DeviceProject)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$device=(Resolve-Path -LiteralPath $DeviceProject).Path
$boot=(Resolve-Path -LiteralPath (Join-Path $device '..\RL78_F24_Boot_loader_CAN_FD')).Path
$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC developer tools required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$canonical=Join-Path $repo 'firmware\common\custom_transfer_fd.h'
if([IO.File]::ReadAllText($canonical).Replace("`r`n","`n") -cne [IO.File]::ReadAllText((Join-Path $device 'custom_transfer_fd.h')).Replace("`r`n","`n")){throw 'FD wire header differs.'}
$out=Join-Path $repo 'tmp\internal\tests\fd_transfer'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$core=Join-Path $repo 'src\core'
$sources=@((Join-Path $repo 'tests\fd_transfer_tests.cpp'))
foreach($name in @('host_link_protocol','host_link_client','can_host_client','f13_transfer_profile')){$sources+=Join-Path $core ($name+'.cpp')}
$includes=@(('/I'+$out),('/I'+$core),('/I'+(Join-Path $boot 'tools\tests\transfer_mock')),('/I'+(Join-Path $boot 'common')),('/I'+$device),('/I'+$boot))
& (Join-Path $boot 'tools\tests\prepare_rxmb_model.ps1') -DeviceProject $device -OutputDirectory $out
$probe=Join-Path $out 'rxmb_config_probe.c'
[IO.File]::WriteAllText($probe,@'
#include "f24_device_config.h"
#if CAN_CFG_NRXMB != 1 || CAN_CFG_RMPLS != 7 || CAN_CFG_RMIE_ALL != 2 || CAN_CFG_RMIE_VALUE != 1
#error Transfer_requires_one_64_byte_RXMB_with_only_buffer0_interrupt
#endif
#if CAN_CFG_RF0_RFDC != 0 || CAN_CFG_RF1_RFDC != 0
#error Transfer_RX_FIFOs_must_be_disabled
#endif
#if CAN_CFG_INTRCANGRVC_USE != 1 || CAN_CFG_INTRCANGRFR_USE != 0 || CAN_CFG_INTRCAN0TRM_USE != 0
#error Transfer_requires_RXMB_interrupt_with_FIFO_and_TX_interrupts_disabled
#endif
#if CAN_CFG_NBRP != 3 || CAN_CFG_NMNL_TSEG1 != 15 || CAN_CFG_NMNL_TSEG2 != 4 || CAN_CFG_NMNL_SJW != 1
#error Transfer_nominal_timing_changed
#endif
#if CAN_CFG_DBRP != 1 || CAN_CFG_DATA_TSEG1 != 7 || CAN_CFG_DATA_TSEG2 != 2 || CAN_CFG_DATA_SJW != 1
#error Transfer_data_timing_changed
#endif
#if CAN_CFG_CLOE != 0 || CAN_CFG_FDOE != 1 || BSP_CFG_FMX_HZ != 4000000
#error Transfer_FD_mode_or_board_clock_changed
#endif
'@,(New-Object Text.UTF8Encoding($false)))
& cl /nologo /EP /DF24_DEVICE_TRANSFER ('/I'+(Join-Path $boot 'common')) ('/I'+(Join-Path $device 'src\smc_gen\r_config')) $probe | Out-Null
if($LASTEXITCODE){throw 'Actual Transfer configuration preprocessing failed.'}
Write-Host 'PASS: actual Transfer RXMB-only configuration and unchanged FD mode/timing.'
Push-Location -LiteralPath $out
try {
    & cl /nologo /TC /Za /W4 /WX /DF24_DEVICE_TRANSFER @includes (Join-Path $boot 'tools\tests\fd_can_tests.c') /Fe:fd_runtime_hal_tests.exe
    if($LASTEXITCODE){throw 'FD runtime HAL model build failed.'}
    & (Join-Path $out 'fd_runtime_hal_tests.exe')
    if($LASTEXITCODE){throw 'FD runtime HAL model failed.'}
    foreach($trace in @(0,1)) {
        & cl /nologo /TC /Za /W4 /WX ("/DCAN_TRANSFER_UART_TRACE_ENABLE="+$trace) @includes (Join-Path $device 'can_transfer_protocol.c') /Fo:fd_protocol.obj /c
        if($LASTEXITCODE){throw 'FD C90 model compile failed.'}
        & cl /nologo /std:c++17 /W4 /WX /EHsc ("/DCAN_TRANSFER_UART_TRACE_ENABLE="+$trace) @includes @sources fd_protocol.obj /Fe:fd_transfer_tests.exe
        if($LASTEXITCODE){throw 'FD native client test build failed.'}
        $log=Join-Path $out ("trace"+$trace+'.log')
        & (Join-Path $out 'fd_transfer_tests.exe') *> $log
        if($LASTEXITCODE){Get-Content -LiteralPath $log -Tail 35;throw 'FD Transfer model failed.'}
        Get-Content -LiteralPath $log -Tail 1
        Write-Host "PASS: FD Transfer models trace=$trace; hardware NOT RUN."
    }
}finally{Pop-Location}
