param([Parameter(Mandatory=$true)][string]$DeviceRoot,
      [ValidateSet('F13','F24','G16','G22')][string[]]$Mcu=@('F13','F24','G16','G22'),
      [ValidateSet('UART','I2C')][string]$Bus='UART')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root ('tmp\internal\tests\isp_client_'+$Bus)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$common=Join-Path $root 'firmware\common\isp_device';$core=Join-Path $root 'src\core'
Push-Location -LiteralPath $out
try {
    foreach($mcu in $Mcu) {
        $boot=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_'+$Bus)
        & cl /nologo /TC /W4 /WX /c ('/I'+$common) ('/I'+$boot) (Join-Path $common 'isp_protocol.c') (Join-Path $common 'isp_device.c') (Join-Path $common 'isp_i2c_mailbox.c') (Join-Path $boot 'image_metadata.c') (Join-Path $boot 'image_metadata_profile.c')
        if($LASTEXITCODE){throw 'Actual device C compilation failed.'}
        & cl /nologo /std:c++17 /EHsc /W4 /WX ('/I'+$common) ('/I'+$core) ('/I'+$boot) (Join-Path $root 'tests\isp_client_tests.cpp') (Join-Path $core 'isp_device_client.cpp') (Join-Path $core 'firmware_update_profile.cpp') isp_protocol.obj isp_device.obj isp_i2c_mailbox.obj image_metadata.obj image_metadata_profile.obj /Fe:isp_client_tests.exe
        if($LASTEXITCODE){throw 'Production ISP client compilation failed.'}
        & '.\isp_client_tests.exe' (Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_'+$Bus+'_app\DefaultBuild\RL78_'+$mcu+'_ISP_Boot_loader_'+$Bus+'_app_update.bin'))
        if($LASTEXITCODE){throw ($mcu+' ISP client model failed.')}
    }
} finally {Pop-Location}
