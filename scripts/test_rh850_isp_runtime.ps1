param([Parameter(Mandatory=$true)][string]$DeviceRoot,
    [ValidateSet('UART','I2C','Xmodem')][string[]]$Bus=@('UART','I2C','Xmodem'),
    [ValidateSet('BOOT','APP','TRANSFER')][string[]]$Role=@('BOOT','APP','TRANSFER'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\rh850_isp_runtime'
[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
Push-Location -LiteralPath $out
try{foreach($selectedBus in $Bus){
    $name='Sample_Project_RH850_S1_ISP_Boot_loader_'+$selectedBus
    $common=Join-Path $DeviceRoot ($name+'\common')
    $text=[IO.File]::ReadAllText((Join-Path $common 'isp_rh850_runtime.c'))
    $globals=$text.IndexOf('static IspDevice g_device;')
    $hardware=$text.IndexOf('unsigned short IspTick(void)')
    $begin=$text.IndexOf('static unsigned char Metadata(')
    $end=$text.IndexOf('static unsigned char RuntimeStart(void)')
    if($globals -lt 0 -or $hardware -le $globals -or $begin -le $hardware -or $end -le $begin){throw 'Runtime extraction anchors changed.'}
    # Preserve actual state/config, Action, Flash/stream adapters and RuntimePoll.
    # Only target clock/startup, log registers and reset implementation are boundaries.
    [IO.File]::WriteAllText((Join-Path $out 'actual_runtime.inc'),
        $text.Substring($globals,$hardware-$globals)+$text.Substring($begin,$end-$begin),(New-Object Text.UTF8Encoding($false)))
    foreach($selectedRole in $Role){
        $defines=@('/DISP_DEVICE_'+$selectedRole)
        if($selectedBus -eq 'I2C'){$defines+='/DISP_I2C'}elseif($selectedBus -eq 'Xmodem'){$defines+='/DISP_XMODEM'}
        $files=@('isp_protocol.c','isp_device.c','device_image.c','image_metadata.c','image_metadata_profile.c')
        if($selectedBus -eq 'Xmodem'){$files+='isp_xmodem.c'}
        $sources=@($files|ForEach-Object {Join-Path $common $_})
        $profile=[IO.File]::ReadAllText((Join-Path $common 'image_metadata_profile.c'))
        $profile=[regex]::Replace($profile,'(?m)^#pragma section[^\r\n]*','')
        [IO.File]::WriteAllText((Join-Path $out 'image_metadata_profile.c'),$profile,(New-Object Text.UTF8Encoding($false)))
        $sources=@($sources|ForEach-Object {if([IO.Path]::GetFileName($_) -eq 'image_metadata_profile.c'){Join-Path $out 'image_metadata_profile.c'}else{$_}})
        $identity=Join-Path $DeviceRoot ($name+'_app\generated_projection')
        & cl /nologo /TC /W4 /WX /c @defines ('/I'+$common) ('/I'+$identity) ('/I'+$out) @sources (Join-Path $root 'tests\rh850_isp_runtime_harness.c')
        if($LASTEXITCODE){throw ($selectedBus+'/'+$selectedRole+' actual runtime C compilation failed.')}
        $objects=@($files|ForEach-Object {[IO.Path]::ChangeExtension($_,'.obj')})+@('rh850_isp_runtime_harness.obj')
        $core=Join-Path $root 'src\core'
        & cl /nologo /std:c++17 /EHsc /W4 /WX @defines ('/I'+$common) ('/I'+$core) (Join-Path $root 'tests\rh850_isp_runtime_tests.cpp') (Join-Path $core 'isp_device_client.cpp') (Join-Path $core 'firmware_update_profile.cpp') @objects /Fe:rh850_isp_runtime_tests.exe
        if($LASTEXITCODE){throw 'Runtime client model compilation failed.'}
        & .\rh850_isp_runtime_tests.exe (Join-Path $DeviceRoot ($name+'_app\DefaultBuild\'+$name+'_app_update.bin'))
        if($LASTEXITCODE){throw ($selectedBus+'/'+$selectedRole+' runtime integration failed.')}
        Write-Host ('PASS '+$selectedBus+'/'+$selectedRole+' actual runtime integration;hardware NOT RUN.')
    }
}}finally{Pop-Location}
