param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'rl78_entrypoints'
$out=$scope.Path
try {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $header=@'
#ifndef ISP_RL78_BOARD_H
#define ISP_RL78_BOARD_H
#define __far
extern unsigned long mock_ram[4];
#define BOOT_APP_SHARED_RAM_ADDRESS ((size_t)mock_ram)
#define BOOT_APP_HANDOFF_SIGNATURE 0xABCDEF01UL
#define BOOT_APP_UPDATE_COMPLETE_SIGNATURE 0x12345678UL
#define BOOT_APP_RESET_TO_BOOT_SIGNATURE 0x99887766UL
#define ISP_BOOT_IDLE_TIMEOUT_MS 3000U
#define ISP_MODE_BOOT 0U
#define ISP_MODE_APP 1U
#define ISP_MODE_TRANSFER 2U
unsigned char IspBoardInit(void);
void IspBoardReset(void);
void Device_Jump(void);
unsigned char IspInstalledValid(void);
void IspRuntimeInit(unsigned char mode);
void IspRuntimePoll(void);
void IspRuntimeHandoff(void);
unsigned char IspRuntimeActive(void);
unsigned short IspRuntimeIdle(void);
unsigned short IspTick(void);
void IspLog(const char *text);
#endif
'@
    [IO.File]::WriteAllText((Join-Path $out 'isp_rl78_board.h'),("#include <stddef.h>`n"+$header))
    foreach($name in @('platform.h','r_cg_macrodriver.h')){[IO.File]::WriteAllText((Join-Path $out $name),'/* Board APIs are mocked by the harness. */')}
    Push-Location -LiteralPath $out
    try {
        foreach($mcu in @('F13','F24','G16','G22')){foreach($bus in @('UART','I2C','Xmodem')){foreach($role in @('BOOT','APP','TRANSFER')){
            $base='RL78_'+$mcu+'_ISP_Boot_loader_'+$bus
            $name=switch($role){BOOT {$base} APP {$base+'_app'} TRANSFER {'RL78_'+$mcu+'_'+$(if($bus -eq 'I2C'){'I2C_Slave'}else{$bus})+'_Transfer'}}
            $folder=Join-Path $DeviceRoot $name
            $body=[IO.File]::ReadAllText((Join-Path $folder 'custom_func.c'))
            $marker='/* Project initialization and recurring work. */'
            $at=$body.IndexOf($marker,[StringComparison]::Ordinal)
            if($at -lt 0){throw 'Missing actual user callback boundary.'}
            $actual=Join-Path $out 'actual_entry.c'
            [IO.File]::WriteAllText($actual,$body.Substring($at+$marker.Length))
            $exe=Join-Path $out ('entrypoint_'+$mcu+'_'+$bus+'_'+$role+'.exe')
            & cl /nologo /TC /Za /W3 ('/DISP_MCU_'+$mcu) ('/DISP_DEVICE_'+$role) ('/I'+$out) ('/I'+$folder) $actual (Join-Path $root 'tests\rl78_entrypoint_harness.c') ('/Fe:'+$exe) *> (Join-Path $out 'compile.log')
            if($LASTEXITCODE){Get-Content -LiteralPath (Join-Path $out 'compile.log') -Tail 12;throw ('Actual entry compilation failed: '+$name)}
            & $exe
            if($LASTEXITCODE){throw ('Actual entry execution failed: '+$name)}
            Write-Host ('PASS '+$mcu+'/'+$bus+'/'+$role+' callbacks; hardware NOT RUN.')
        }}}
    } finally { Pop-Location }
} finally { Remove-OwnedTestWorkspace $scope }
