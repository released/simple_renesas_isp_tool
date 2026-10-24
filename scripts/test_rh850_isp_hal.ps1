param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\rh850_isp_hal';[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC is required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$portable=Join-Path $root 'firmware\common\isp_device'
$utf8=New-Object Text.UTF8Encoding($false)
Push-Location -LiteralPath $out
try{foreach($selectedBus in @('UART','I2C')){
    $project=Join-Path $DeviceRoot ('Sample_Project_RH850_S1_ISP_Boot_loader_'+$selectedBus)
    $file=if($selectedBus -eq 'I2C'){'isp_rh850_i2c.c'}else{'isp_rh850_uart.c'}
    $body=[IO.File]::ReadAllText((Join-Path $project ('common\'+$file)))
    $body=[regex]::Replace($body,'(?m)^\s*(#include|#pragma interrupt)[^\r\n]*','')
    [IO.File]::WriteAllText((Join-Path $out 'actual_hal.inc'),$body,$utf8)
    $vendor=Join-Path $project ('src\smc_gen\general\'+$(if($selectedBus -eq 'I2C'){'r_cg_riic.h'}else{'r_cg_uart.h'}))
    $bits=@([regex]::Matches([IO.File]::ReadAllText($vendor),'(?m)^#define\s+_(?:RIIC|UART)_[^\r\n]+')|ForEach-Object {$_.Value})
    # Include literal macros only; multiline descriptions are not instructions.
    $bits=@($bits|Where-Object {$_ -match '^#define\s+\w+\s+\(0x[0-9A-Fa-f]+[UL]*\)'})
    $bits=@($bits|ForEach-Object {$_ -replace '\s*/\*.*$',''})
    $bits+=@('#define ISP_I2C_ADDRESS (0x20U)','#define ISP_BUS_RATE (115200UL)')
    [IO.File]::WriteAllText((Join-Path $out 'vendor_hal_bits.h'),($bits -join "`n"),$utf8)
    & cl /nologo /TC /W4 /WX /c ('/I'+$portable) (Join-Path $portable 'isp_i2c_mailbox.c') (Join-Path $portable 'isp_protocol.c')
    if($LASTEXITCODE){throw 'Mailbox C compilation failed.'}
    $defines=@('/wd4505')+$(if($selectedBus -eq 'I2C'){@('/DTEST_I2C')}else{@()})
    & cl /nologo /std:c++17 /EHsc /W4 /WX @defines ('/I'+$portable) ('/I'+$out) (Join-Path $root 'tests\rh850_isp_hal_tests.cpp') isp_i2c_mailbox.obj isp_protocol.obj /Fe:rh850_isp_hal_tests.exe
    if($LASTEXITCODE){throw 'Actual adapter transition compilation failed.'}
    & .\rh850_isp_hal_tests.exe
    if($LASTEXITCODE){throw ($selectedBus+' HAL transition model failed.')}
}}finally{Pop-Location}
