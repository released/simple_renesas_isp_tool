param([Parameter(Mandatory=$true)][string]$DeviceRoot,
      [ValidateSet('F13','F24','G16','G22')][string[]]$Mcu=@('F13','F24','G16','G22'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\isp_xmodem_client'
[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$common=Join-Path $root 'firmware\common\isp_device';$core=Join-Path $root 'src\core'
Push-Location -LiteralPath $out
try{
    foreach($mcu in $Mcu){
        $boot=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_Xmodem')
        $defines=@('/DISP_XMODEM','/DISP_DEVICE_BOOT','/DISP_TEST_DEVICE_CONTEXT','/DISP_TEST_XMODEM_CONTEXT','/DISP_TEST_XMODEM_DIRECT','/DISP_TEST_FIXED_PROFILE')
        & cl /nologo /TC /W4 /WX /c @defines ('/I'+$common) ('/I'+$boot) (Join-Path $common 'isp_protocol.c') (Join-Path $common 'isp_device.c') (Join-Path $common 'isp_xmodem.c') (Join-Path $boot 'image_metadata.c') (Join-Path $boot 'image_metadata_profile.c')
        if($LASTEXITCODE){throw 'Actual Xmodem Boot C compilation failed.'}
        & cl /nologo /std:c++17 /W4 /WX /EHsc @defines ('/I'+$common) ('/I'+$core) ('/I'+$boot) (Join-Path $root 'tests\isp_xmodem_client_tests.cpp') (Join-Path $core 'isp_device_client.cpp') (Join-Path $core 'firmware_update_profile.cpp') isp_protocol.obj isp_device.obj isp_xmodem.obj image_metadata.obj image_metadata_profile.obj /Fe:isp_xmodem_client_tests.exe
        if($LASTEXITCODE){throw 'Production Xmodem client test compilation failed.'}
        $bin=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_Xmodem_app\DefaultBuild\RL78_'+$mcu+'_ISP_Boot_loader_Xmodem_app_update.bin')
        & '.\isp_xmodem_client_tests.exe' $bin
        if($LASTEXITCODE){throw ($mcu+' Xmodem integration model failed.')}
        # Include the production transport loop too, not just engine/receiver.
        & cl /nologo /TC /W4 /WX /c @defines ('/I'+$common) ('/I'+$boot) (Join-Path $root 'tests\isp_xmodem_runtime_harness.c')
        if($LASTEXITCODE){throw 'Boot runtime harness compilation failed.'}
        & cl /nologo /std:c++17 /W4 /WX /EHsc @defines /DISP_TEST_RUNTIME ('/I'+$common) ('/I'+$core) ('/I'+$boot) (Join-Path $root 'tests\isp_xmodem_client_tests.cpp') (Join-Path $core 'isp_device_client.cpp') (Join-Path $core 'firmware_update_profile.cpp') isp_protocol.obj isp_device.obj isp_xmodem.obj image_metadata.obj image_metadata_profile.obj isp_xmodem_runtime_harness.obj /Fe:isp_xmodem_boot_runtime_tests.exe
        if($LASTEXITCODE){throw 'Boot runtime/client compilation failed.'}
        & '.\isp_xmodem_boot_runtime_tests.exe' $bin
        if($LASTEXITCODE){throw ($mcu+' Xmodem actual Boot runtime failed.')}
        foreach($runtimeRole in @('APP','TRANSFER')){
            $runtimeDefines=@('/DISP_XMODEM',('/DISP_DEVICE_'+$runtimeRole),'/DISP_TEST_XMODEM_CONTEXT','/DISP_TEST_XMODEM_DIRECT')
            & cl /nologo /TC /W4 /WX /c @runtimeDefines ('/I'+$common) ('/I'+(Join-Path $root 'tests')) (Join-Path $common 'isp_protocol.c') (Join-Path $common 'isp_device.c') (Join-Path $common 'isp_xmodem.c') (Join-Path $root 'tests\isp_xmodem_runtime_harness.c')
            if($LASTEXITCODE){throw 'Actual Xmodem App/Transfer runtime C compilation failed.'}
            & cl /nologo /std:c++17 /W4 /WX /EHsc @runtimeDefines ('/I'+$common) ('/I'+$core) (Join-Path $root 'tests\isp_xmodem_runtime_tests.cpp') (Join-Path $core 'isp_device_client.cpp') (Join-Path $core 'firmware_update_profile.cpp') isp_protocol.obj isp_device.obj isp_xmodem.obj isp_xmodem_runtime_harness.obj /Fe:isp_xmodem_runtime_tests.exe
            if($LASTEXITCODE){throw 'Production runtime/client test compilation failed.'}
            & '.\isp_xmodem_runtime_tests.exe' $bin
            if($LASTEXITCODE){throw ($mcu+' Xmodem '+$runtimeRole+' runtime model failed.')}
        }
    }
}finally{Pop-Location}
Write-Host 'PASS Xmodem production PC client/actual C receiver, Boot gates and App/Transfer runtime CRC sink. Hardware NOT RUN.'
