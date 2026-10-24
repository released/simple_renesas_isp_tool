param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\isp_rl78_uart_hal';[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Existing MSVC is required.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$common=Join-Path $root 'firmware\common\isp_device'
Push-Location -LiteralPath $out
try{foreach($mcu in @('F13','F24','G16','G22')){
    $name=if($mcu -in @('F13','F24')){'isp_rl78_board.c'}else{'isp_board.c'}
    $source=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_UART\isp\'+$name)
    $body=[IO.File]::ReadAllText($source);$start=$body.IndexOf('static volatile unsigned short g_tick;')
    $end=if($mcu -in @('F13','F24')){$body.IndexOf('static void uart_init(void)')}else{$body.IndexOf('void IspLog(')}
    if($start -lt 0 -or $end -le $start){throw 'UART production extraction anchors changed.'}
    $body=$body.Substring($start,$end-$start)
    $body=[regex]::Replace($body,'(?m)^#pragma interrupt[^\r\n]*','')
    [IO.File]::WriteAllText((Join-Path $out 'actual_rl78_uart.inc'),$body,(New-Object Text.UTF8Encoding($false)))
    & cl /nologo /TC /W4 /WX /DISP_DEVICE_APP ('/I'+$common) ('/I'+$out) (Join-Path $root 'tests\isp_rl78_uart_hal_tests.c') (Join-Path $common 'isp_protocol.c') /Fe:isp_rl78_uart_hal_tests.exe
    if($LASTEXITCODE){throw ($mcu+' UART diagnostic model compile failed.')}
    & .\isp_rl78_uart_hal_tests.exe;if($LASTEXITCODE){throw ($mcu+' UART diagnostic model failed.')}
    Write-Host ('PASS '+$mcu+' actual UART ISR/diagnostics.')
}}finally{Pop-Location}
