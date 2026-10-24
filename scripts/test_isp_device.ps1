param([string]$DeviceRoot='',
      [ValidateSet('F13','F24','G16','G22')][string[]]$Mcu=@('F13','F24','G16','G22'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$DeviceRoot){$DeviceRoot=Split-Path -Parent $root}
$out=Join-Path $root 'tmp\internal\tests\isp_device'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required for the native model.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$common=Join-Path $root 'firmware\common\isp_device'
Push-Location -LiteralPath $out
try {
    foreach($mcu in $Mcu) {
        $boot=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_UART')
        foreach($binding in 'callbacks','fixed') {
            [string[]]$defines=@()
            if($binding -eq 'fixed'){$defines=@('/DISP_TEST_FIXED_PROFILE')}
            & cl /nologo /TC /W4 /WX /c @defines ('/I'+$common) ('/I'+$boot) (Join-Path $common 'isp_protocol.c') (Join-Path $common 'isp_device.c') (Join-Path $boot 'image_metadata.c') (Join-Path $boot 'image_metadata_profile.c')
            if($LASTEXITCODE){throw 'Production C model compilation failed.'}
            & cl /nologo /std:c++17 /W4 /WX /EHsc ('/I'+$common) ('/I'+$boot) (Join-Path $root 'tests\isp_device_tests.cpp') isp_protocol.obj isp_device.obj image_metadata.obj image_metadata_profile.obj /Fe:isp_device_tests.exe
            if($LASTEXITCODE){throw 'Test compilation failed.'}
            $app=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_UART_app')
            $bin=Join-Path $app ('DefaultBuild\RL78_'+$mcu+'_ISP_Boot_loader_UART_app_update.bin')
            if(!(Test-Path -LiteralPath $bin -PathType Leaf)){throw ('Build the matching App before actual-BIN testing: '+$bin)}
            & '.\isp_device_tests.exe' $bin
            if($LASTEXITCODE){throw 'ISP device model failed.'}
            Write-Host ('PASS '+$mcu+' '+$binding+' geometry + actual independent metadata parser/profile')
        }
        foreach($role in 'BOOT','APP','TRANSFER') {
            $define='/DISP_DEVICE_'+$role
            & cl /nologo /TC /W4 /WX /c $define ('/I'+$common) ('/I'+$boot) (Join-Path $common 'isp_protocol.c') (Join-Path $common 'isp_device.c')
            if($LASTEXITCODE){throw 'Role C compilation failed.'}
            & cl /nologo /std:c++17 /W4 /WX /EHsc $define ('/I'+$common) ('/I'+$boot) (Join-Path $root 'tests\isp_role_tests.cpp') isp_protocol.obj isp_device.obj /Fe:isp_role_tests.exe
            if($LASTEXITCODE){throw 'Role test compilation failed.'}
            & '.\isp_role_tests.exe'
            if($LASTEXITCODE){throw 'Compiled role gates failed.'}
            Write-Host ('PASS '+$mcu+' compiled '+$role)
        }
    }
} finally { Pop-Location }
Write-Host 'PASS: production framing/update C with native callbacks. Hardware NOT RUN.'
