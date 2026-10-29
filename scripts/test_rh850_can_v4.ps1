param([Parameter(Mandatory=$true)][string]$BootProject,[switch]$DeviceAdapter,[switch]$ClassicalAdapter,[switch]$Matrix)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
$project=(Resolve-Path -LiteralPath $BootProject).Path
$common=$project
$v4=Join-Path $project 'hal\can'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Existing MSVC developer tools required for native models only.'}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue) -or !$env:VCToolsInstallDir -or !$env:INCLUDE -or !$env:LIB) {
    $envLines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $envLines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$testScope=New-OwnedTestWorkspace 'rh850_can_v4'
$out=$testScope.Path
try {
$masks=if($Matrix){@(1,2,16,18,63)}else{@(18)}
$boardFunctions=@('Prepare','Start','Stop','StopChannel','StartChannel','Millis','Lock','Unlock','EnableRxIrq','AckRxIrq',
    'TakeFifoLost','ReadFifo','ReadLatest','Send','TxResult','Abort','Status','ClearErrors')
Push-Location -LiteralPath $out
try {
    foreach($mask in $masks){foreach($fd in @(0,1)){
        if($ClassicalAdapter -and $fd){continue}
        $defs=@('/DCAN_NATIVE_TEST',('/DRH850_CAN_FD='+$fd))
        for($ch=0;$ch -lt 6;$ch++){$defs+=('/DENABLE_PORT_CAN'+$ch+'='+[int](($mask -band (1 -shl $ch)) -ne 0))}
        if($DeviceAdapter){$defs+='/DTEST_DEVICE_ADAPTER'}
        if($ClassicalAdapter){$defs+='/DTEST_CLASSICAL_ADAPTER'}
        $includes=@(('/I'+(Join-Path $repo 'tests\rh850_v4_mock')),('/I'+$v4),('/I'+$common),('/I'+$project),('/I'+(Join-Path $project 'hal\device')),('/I'+(Join-Path $project 'hal\image')),('/I'+(Join-Path $project 'hal\isp')))
        $rename=@($boardFunctions|ForEach-Object {'/DCanBoard_'+$_+'=Actual_'+$_})
        & cl /nologo /TC /Za /W4 /WX /wd4068 @defs @rename @includes /c (Join-Path $v4 'can_board.c') /Fo:actual_board.obj
        if($LASTEXITCODE){throw 'Production EVM board model compile failed.'}
        $sources=@('can_service.c','can_config.c','rs_canfd.c','rs_canfd_validate.c')|ForEach-Object {Join-Path $v4 $_}
        $sources+=Get-DeviceProjectSource $common 'rh850_can_ports.c'
        if($DeviceAdapter){$sources+=Get-DeviceProjectSource $common 'device_can.c'}
        if($ClassicalAdapter){$sources+=Get-DeviceProjectSource $project 'rh850_can.c'}
        & cl /nologo /Za /W4 /WX /wd4068 @defs @includes @sources (Join-Path $repo 'tests\rh850_can_v4_tests.c') actual_board.obj /Fe:rh850_can_v4_tests.exe
        if($LASTEXITCODE){throw 'Production v4/ISP C90 model compile failed.'}
        & .\rh850_can_v4_tests.exe $fd
        if($LASTEXITCODE){throw ('v4/ISP model failed: mask='+$mask+' FD='+$fd)}
    }}
} finally {Pop-Location}
Write-Host ('PASS actual v4 core/driver + EVM adapter + ISP wrapper; modeled hardware only. Evidence: '+$out)

} finally { Remove-OwnedTestWorkspace $testScope }
