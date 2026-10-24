param([Parameter(Mandatory=$true)][string]$DeviceRoot,[ValidateSet('UART','I2C','Xmodem')][string[]]$Bus=@('UART','I2C','Xmodem'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\rh850_isp';[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
Push-Location -LiteralPath $out
try{foreach($selectedBus in $Bus){
    $name='Sample_Project_RH850_S1_ISP_Boot_loader_'+$selectedBus
    $common=Join-Path $DeviceRoot ($name+'\common');$core=Join-Path $root 'src\core'
    $runtime=[IO.File]::ReadAllText((Join-Path $common 'isp_rh850_runtime.c'))
    $start=$runtime.IndexOf('static unsigned char Metadata(');$end=$runtime.IndexOf('static void Action(void)')
    if($start -lt 0 -or $end -lt $start){throw 'Actual adapter extraction anchors changed.'}
    $adapter=@'
#include <string.h>
#include "isp_device.h"
#include "device_image.h"
unsigned char Device_FlashInit(void);
unsigned char Device_FlashErase(unsigned long at);
unsigned char Device_FlashWritePage(unsigned long at,const unsigned char *bytes);
unsigned char Device_FlashRead(unsigned long at,unsigned char *bytes,unsigned short size);
unsigned char IspBusSend(const unsigned char *bytes,unsigned short size);
static IspDevice g_device;
static unsigned char g_pending;
'@+"`n"+$runtime.Substring($start,$end-$start)
    if($selectedBus -eq 'Xmodem'){
        $start=$runtime.IndexOf('static unsigned char StreamWrite(');$end=$runtime.IndexOf('static void RuntimePoll(void)')
        if($start -lt 0 -or $end -lt $start){throw 'Actual X adapter anchors changed.'}
        $body=$runtime.Substring($start,$end-$start)
        # This span ends at the enclosing ISP_XMODEM conditional.
        $body=[regex]::Replace($body,'#endif\s*\z','')
        $adapter+="`n#include `"isp_xmodem.h`"`nstatic IspXmodem g_x;`n"+$body
        $adapter+="`nvoid RhIspStreamInit(unsigned long size){IspXmodemInit(&g_x,size,0U);}`nvoid RhIspStreamFeed(unsigned char byte,unsigned short tick){IspXmodemFeed(&g_x,byte,tick,&g_x_ops);}`nvoid RhIspStreamPoll(unsigned short tick){IspXmodemPoll(&g_x,tick,&g_x_ops);}`n"
    }else{
        $adapter+="`nvoid RhIspStreamInit(unsigned long size){(void)size;}`nvoid RhIspStreamFeed(unsigned char byte,unsigned short tick){(void)byte;(void)tick;}`nvoid RhIspStreamPoll(unsigned short tick){(void)tick;}`n"
    }
    $adapter+="`nvoid RhIspInit(const IspDeviceConfig *c){g_flash_ready=0U;g_page_used=0U;g_pending=0U;IspDeviceInit(&g_device,c,&g_ops,ISP_MODE_BOOT);}`nvoid RhIspProcess(const IspPacket *q,IspPacket *a){IspDeviceProcess(&g_device,q,a);}`nunsigned char RhIspAction(void){return g_device.action;}`n"
    [IO.File]::WriteAllText((Join-Path $out 'actual_flash_adapter.c'),$adapter,(New-Object Text.UTF8Encoding($false)))
    $defines=@('/DISP_DEVICE_BOOT')+$(if($selectedBus -eq 'Xmodem'){@('/DISP_XMODEM')}else{@()})
    $sources=@('isp_protocol.c','isp_device.c','device_image.c','image_metadata.c','image_metadata_profile.c')|ForEach-Object {Join-Path $common $_}
    $portable=Join-Path $root 'firmware\common\isp_device'
    $sources+=Join-Path $portable 'isp_i2c_mailbox.c'
    if($selectedBus -eq 'Xmodem'){$sources+=Join-Path $common 'isp_xmodem.c'}
    & cl /nologo /TC /W4 /WX /c @defines ('/I'+$common) ('/I'+$portable) @sources actual_flash_adapter.c
    if($LASTEXITCODE){throw 'Actual RH850 C engine/page adapter compilation failed.'}
    $objects=@('isp_protocol.obj','isp_device.obj','device_image.obj','image_metadata.obj','image_metadata_profile.obj','isp_i2c_mailbox.obj','actual_flash_adapter.obj')
    if($selectedBus -eq 'Xmodem'){$objects+='isp_xmodem.obj'}
    & cl /nologo /std:c++17 /EHsc /W4 /WX @defines ('/I'+$common) ('/I'+$portable) ('/I'+$core) (Join-Path $root 'tests\rh850_isp_tests.cpp') (Join-Path $core 'isp_device_client.cpp') (Join-Path $core 'firmware_update_profile.cpp') @objects /Fe:rh850_isp_tests.exe
    if($LASTEXITCODE){throw 'Production RH850 ISP client model compilation failed.'}
    $bin=Join-Path $DeviceRoot ($name+'_app\DefaultBuild\'+$name+'_app_update.bin')
    & .\rh850_isp_tests.exe $bin
    if($LASTEXITCODE){throw ($selectedBus+' RH850 ISP model failed.')}
}}finally{Pop-Location}
Write-Host 'PASS RH850 serial native client/C-engine/page models. No physical UART/RIIC/FCL acceptance.'
