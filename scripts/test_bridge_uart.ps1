param()
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\bridge_uart'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required for native verification.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$common=Join-Path $root 'firmware\common'
$isp=Join-Path $common 'isp_device'
$core=Join-Path $root 'src\core'
Push-Location -LiteralPath $out
try {
    & cl /nologo /TC /W4 /WX /c /DISP_DEVICE_TRANSFER ('/I'+$common) ('/I'+$isp) (Join-Path $common 'bridge_uart.c') (Join-Path $isp 'isp_protocol.c') (Join-Path $isp 'isp_device.c')
    if($LASTEXITCODE){throw 'Production C compilation failed.'}
    & cl /nologo /std:c++17 /EHsc /W4 /WX ('/I'+$common) ('/I'+$isp) ('/I'+$core) (Join-Path $root 'tests\bridge_uart_tests.cpp') (Join-Path $core 'uart_host_client.cpp') (Join-Path $core 'bridge_port_catalog.cpp') (Join-Path $core 'host_link_client.cpp') (Join-Path $core 'host_link_protocol.cpp') bridge_uart.obj isp_protocol.obj isp_device.obj /Fe:bridge_uart_tests.exe
    if($LASTEXITCODE){throw 'Production client/test compilation failed.'}
    & '.\bridge_uart_tests.exe'
    if($LASTEXITCODE){throw 'Bridge UART model failed.'}
    & cl /nologo /TC /W4 /WX /wd4068 /DBRIDGE_UART_BACKEND_TEST ('/I'+$common) ('/I'+(Join-Path $root 'tests')) (Join-Path $root 'tests\f24_uart_backend_tests.c') (Join-Path $root 'firmware\f24_board\f24_uart_backend.c') /Fe:f24_uart_backend_tests.exe
    if($LASTEXITCODE){throw 'Actual F24 backend model compilation failed.'}
    & '.\f24_uart_backend_tests.exe'
    if($LASTEXITCODE){throw 'Actual F24 backend model failed.'}
    # Shared port catalog changed; exercise its existing CAN/I2C/G16 callers
    # and the independent UART0 backend, not unrelated UI/update suites.
    & cl /nologo /TC /W4 /WX /wd4068 /c ('/I'+$common) ('/I'+(Join-Path $root 'tests')) (Join-Path $root 'tests\bridge_i2c_tests.c') (Join-Path $root 'tests\f24_i2c_backend_tests.c') (Join-Path $root 'tests\f24_uart_tests.c') (Join-Path $common 'bridge_i2c.c')
    if($LASTEXITCODE){throw 'Existing transport regression C compilation failed.'}
    & cl /nologo /std:c++17 /EHsc /W4 /WX ('/I'+$common) ('/I'+$core) (Join-Path $root 'tests\bridge_transport_regression.cpp') (Join-Path $root 'tests\multibus_tests.cpp') (Join-Path $core 'bridge_port_catalog.cpp') (Join-Path $core 'i2c_host_client.cpp') (Join-Path $core 'g16_i2c_reference.cpp') (Join-Path $core 'g16_i2c_discovery.cpp') (Join-Path $core 'g16_i2c_image.cpp') (Join-Path $core 'host_link_client.cpp') (Join-Path $core 'host_link_protocol.cpp') bridge_i2c_tests.obj f24_i2c_backend_tests.obj f24_uart_tests.obj bridge_i2c.obj /Fe:bridge_transport_regression.exe
    if($LASTEXITCODE){throw 'Existing transport regression client compilation failed.'}
    & '.\bridge_transport_regression.exe'
    if($LASTEXITCODE){throw 'Existing transport regression failed.'}
} finally { Pop-Location }
