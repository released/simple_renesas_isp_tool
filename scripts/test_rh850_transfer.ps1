param([Parameter(Mandatory=$true)][string]$DeviceProject)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
$device=(Resolve-Path -LiteralPath $DeviceProject).Path
$halIncludes=@('device','image','isp','flash','can')|ForEach-Object {'/I'+(Join-Path $device ('hal\'+$_))}
$fd=$device -match 'CAN_FD'
$defines=@(if($fd){'/DRH850_CAN_FD=1'})
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC developer tools are required for native tests only.'}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue) -or !$env:VCToolsInstallDir -or !$env:INCLUDE -or !$env:LIB) {
    $lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$testScope=New-OwnedTestWorkspace 'rh850_transfer'
$out=$testScope.Path
try {
$core=Join-Path $repo 'src\core'
$sources=@((Join-Path $repo 'tests\rh850_transfer_tests.cpp'))
foreach($name in @('host_link_protocol','host_link_client','can_host_client','f13_transfer_profile')){
    $sources+=Join-Path $core ($name+'.cpp')
}
Push-Location -LiteralPath $out
try{
    $mock=Join-Path $repo 'tests\rh850_mock'
    if(!$fd){
        & (Join-Path $PSScriptRoot 'test_rh850_can_v4.ps1') -BootProject $device -ClassicalAdapter
    }else{
        & (Join-Path $PSScriptRoot 'test_rh850_can_v4.ps1') -BootProject $device -DeviceAdapter
    }
    if($LASTEXITCODE){throw 'Actual v4/Transfer adapter model failed.'}
    & cl /nologo /TC /Za /W4 /WX @defines ('/I'+$device) @halIncludes (Get-DeviceProjectSource $device 'can_transfer_protocol.c') /Fo:rh850_protocol.obj /c
    if($LASTEXITCODE){throw 'RH850 C90 protocol compile failed.'}
    & cl /nologo /std:c++17 /W4 /WX /EHsc @defines ('/I'+$device) @halIncludes ('/I'+$core) @sources rh850_protocol.obj /Fe:rh850_transfer_tests.exe
    if($LASTEXITCODE){throw 'RH850 / production PC client model build failed.'}
    & .\rh850_transfer_tests.exe
    if($LASTEXITCODE){throw 'RH850 Transfer model failed.'}
}finally{Pop-Location}

} finally { Remove-OwnedTestWorkspace $testScope }
