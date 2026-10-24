param([string]$DeviceRoot='',[Alias('Mcu')][ValidateSet('F13','F24','G16','G22')][string[]]$SelectedMcu=@('F13','F24','G16','G22'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\isp_transports'
[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$common=Join-Path $root 'firmware\common\isp_device'
Push-Location -LiteralPath $out
try{
    foreach($binding in 'callbacks','direct','context'){
        [string[]]$defines=@()
        if($binding -eq 'direct'){$defines=@('/DISP_TEST_XMODEM_DIRECT')}
        if($binding -eq 'context'){$defines=@('/DISP_TEST_XMODEM_DIRECT','/DISP_TEST_XMODEM_CONTEXT')}
        & cl /nologo /TC /W4 /WX /c @defines ('/I'+$common) (Join-Path $common 'isp_i2c_mailbox.c') (Join-Path $common 'isp_xmodem.c') (Join-Path $common 'isp_protocol.c')
        if($LASTEXITCODE){throw 'Transport C compilation failed.'}
        & cl /nologo /std:c++17 /W4 /WX /EHsc @defines ('/I'+$common) (Join-Path $root 'tests\isp_transport_tests.cpp') isp_i2c_mailbox.obj isp_xmodem.obj isp_protocol.obj /Fe:isp_transport_tests.exe
        if($LASTEXITCODE){throw 'Transport test compilation failed.'}
        & '.\isp_transport_tests.exe';if($LASTEXITCODE){throw 'Transport models failed.'}
        Write-Host ('PASS Xmodem binding: '+$binding)
    }
    if($DeviceRoot){
        foreach($family in $SelectedMcu){
            $mcu=$family
            $boot=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_I2C')
            $projection=Join-Path $boot 'sc_peripheral_config.generated.h'
            $preinclude=@();if(Test-Path -LiteralPath $projection){$preinclude=@('/FI'+$projection)}
            & cl /nologo /TC /W4 /WX @preinclude ('/DISP_MCU_'+$mcu) ('/I'+(Join-Path $root 'tests\isp_i2c_mock')) ('/I'+$common) ('/I'+$boot) ('/I'+(Join-Path $boot 'isp')) (Join-Path $common 'isp_i2c.c') (Join-Path $common 'isp_i2c_mailbox.c') (Join-Path $root 'tests\isp_i2c_hal_tests.c') /Fe:isp_i2c_hal_tests.exe
            if($LASTEXITCODE){throw ('IICA model compile failed: '+$mcu)}
            & '.\isp_i2c_hal_tests.exe';if($LASTEXITCODE){throw ('IICA model failed: '+$mcu)}
        }
    }
}finally{Pop-Location}
