param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo ('tmp\internal\tests\serial_scfg_validation_'+[guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($out)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Existing MSVC tools are required; no installation is performed.'}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue) -or !$env:VCToolsInstallDir){
    $lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
}
Push-Location -LiteralPath $out
try{foreach($mcu in @('G16','G22')){foreach($bus in @('UART','I2C','Xmodem')){
    $boot=Join-Path $DeviceRoot ('RL78_'+$mcu+'_ISP_Boot_loader_'+$bus)
    & (Join-Path $boot 'validate_scfg.ps1') -Project $boot -CheckOnly
    $body=[IO.File]::ReadAllText((Join-Path $boot 'isp\isp_board.c'))
    $start=$body.IndexOf('unsigned char IspBoardInit(void)');$end=$body.IndexOf('void IspBoardReset(void)')
    if($start -lt 0 -or $end -le $start){throw 'Actual board init extraction anchors changed.'}
    [IO.File]::WriteAllText((Join-Path $out 'actual_scfg_board_init.inc'),$body.Substring($start,$end-$start),(New-Object Text.UTF8Encoding($false)))
    $defines=@('/DISP_MCU_'+$mcu);if($bus -eq 'I2C'){$defines+='/DISP_I2C'}
    $clock=if($mcu -eq 'G16'){8000000}else{16000000}
    $pre=@(('/FI'+(Join-Path $boot 'sc_peripheral_config.generated.h')),('/FI'+(Join-Path $boot 'isp_transport_config.h')))
    if($bus -ne 'I2C'){$pre+=@(('/DISP_UART_CLOCK_HZ='+$clock+'UL'),('/FI'+(Join-Path $boot 'isp\isp_uart_timing.h')))}else{$pre+='/DISP_UART_SDR=0'}
    & cl /nologo /TC /Za /W4 /WX @defines @pre ('/I'+$out) (Join-Path $repo 'tests\rl78_small_scfg_board_tests.c') /Fe:board_init_tests.exe
    if($LASTEXITCODE){throw 'C90 board initializer model compile failed.'}
    & .\board_init_tests.exe;if($LASTEXITCODE){throw ($mcu+'/'+$bus+' board init model failed.')}
}}
}finally{Pop-Location}
Write-Host ('PASS six actual board initialization models. Evidence: '+$out)
