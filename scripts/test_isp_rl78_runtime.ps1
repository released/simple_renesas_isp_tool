param([Parameter(Mandatory=$true)][string]$DeviceRoot,
    [ValidateSet('F13','F24','G16','G22')][string[]]$Mcu=@('F13','F24','G16','G22'),
    [ValidateSet('UART','I2C','Xmodem')][string[]]$Bus=@('UART','I2C','Xmodem'),
    [ValidateSet('BOOT','APP','TRANSFER')][string[]]$Role=@('BOOT','APP','TRANSFER'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\isp_rl78_runtime'
[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required; nothing is installed by this script.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$utf8=New-Object Text.UTF8Encoding($false)
Push-Location -LiteralPath $out
try{foreach($family in $Mcu){foreach($protocol in $Bus){
    $name='RL78_'+$family+'_ISP_Boot_loader_'+$protocol
    $boot=Join-Path $DeviceRoot $name;$common=Join-Path $boot 'isp'
    $runtime=Get-ChildItem -LiteralPath $common -Filter '*runtime.c' -File
    if(@($runtime).Count -ne 1){throw 'Expected exactly one actual runtime.'}
    $text=[IO.File]::ReadAllText($runtime.FullName)
    $begin=if($protocol -eq 'Xmodem'){$text.IndexOf('#if defined(ISP_DEVICE_BOOT) && defined(ISP_XMODEM)')}else{$text.IndexOf('static IspDevice g_device;')}
    $end=$text.IndexOf('#if !defined(ISP_DEVICE_TRANSFER)',$begin)
    if($begin -lt 0 -or $end -le $begin){throw 'Actual globals extraction anchors changed.'}
    [IO.File]::WriteAllText((Join-Path $out 'actual_rl78_globals.inc'),$text.Substring($begin,$end-$begin),$utf8)
    $transport=if($protocol -eq 'UART'){$text.Substring($text.IndexOf('void IspRuntimePoll(void)'))}else{[IO.File]::ReadAllText((Join-Path $common 'isp_runtime_transport.inc'))}
    [IO.File]::WriteAllText((Join-Path $out 'actual_rl78_transport.inc'),$transport,$utf8)
    foreach($kind in $Role){
        $defines=@(('/DISP_DEVICE_'+$kind),('/DISP_MCU_'+$family))
        if($protocol -eq 'I2C'){$defines+='/DISP_I2C'}
        if($protocol -eq 'Xmodem'){$defines+=@('/DISP_XMODEM','/DISP_TEST_XMODEM_CONTEXT','/DISP_TEST_XMODEM_DIRECT');if($kind -eq 'BOOT'){$defines+='/DISP_TEST_DEVICE_CONTEXT'}}
        $sources=@('isp_protocol.c','isp_device.c','isp_i2c_mailbox.c')|ForEach-Object {Join-Path $root ('firmware\common\isp_device\'+$_)}
        $sources+=@((Join-Path $boot 'image_metadata.c'),(Join-Path $boot 'image_metadata_profile.c'),(Join-Path $root 'tests\isp_rl78_runtime_harness.c'))
        if($protocol -eq 'Xmodem'){$sources+=Join-Path $common 'isp_xmodem.c'}
        & cl /nologo /TC /W4 /WX /c @defines /D__far= ('/I'+$out) ('/I'+$boot) ('/I'+$common) ('/I'+(Join-Path $root 'firmware\common\isp_device')) @sources
        if($LASTEXITCODE){throw ($family+'/'+$protocol+'/'+$kind+' actual runtime C compile failed.')}
        $objects=@($sources|ForEach-Object {[IO.Path]::GetFileNameWithoutExtension($_)+'.obj'})
        $core=Join-Path $root 'src\core'
        & cl /nologo /std:c++17 /EHsc /W4 /WX @defines ('/I'+$boot) ('/I'+$common) ('/I'+$core) (Join-Path $root 'tests\isp_rl78_runtime_tests.cpp') (Join-Path $core 'isp_device_client.cpp') (Join-Path $core 'firmware_update_profile.cpp') @objects /Fe:isp_rl78_runtime_tests.exe
        if($LASTEXITCODE){throw 'Runtime/client C++ compile failed.'}
        & .\isp_rl78_runtime_tests.exe (Join-Path $DeviceRoot ($name+'_app\DefaultBuild\'+$name+'_app_update.bin'))
        if($LASTEXITCODE){throw ($family+'/'+$protocol+'/'+$kind+' actual runtime failed.')}
        Write-Host ('PASS '+$family+'/'+$protocol+'/'+$kind+' runtime; board hardware NOT RUN.')
    }
}}}finally{Pop-Location}
