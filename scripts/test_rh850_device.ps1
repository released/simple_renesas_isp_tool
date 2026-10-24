param([Parameter(Mandatory=$true)][string]$BootProject,[Parameter(Mandatory=$true)][string]$AppProject)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot;$boot=(Resolve-Path -LiteralPath $BootProject).Path;$app=(Resolve-Path -LiteralPath $AppProject).Path
$fd=$boot -match 'CAN_FD';$name=Split-Path -Leaf $app
$image=Join-Path $app ('DefaultBuild\'+$name+'_update.bin')
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue) -or !$env:VCToolsInstallDir -or !$env:INCLUDE -or !$env:LIB) {
    $lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
}
$out=Join-Path $repo ('tmp\internal\tests\rh850_device_'+$(if($fd){'fd'}else{'classic'}));New-Item -ItemType Directory -Path $out -Force | Out-Null
$core=Join-Path $repo 'src\core';$common=Join-Path $boot 'common'
$defines=@(if($fd){'/DRH850_CAN_FD=1'})
Push-Location -LiteralPath $out
try{
    & (Join-Path $PSScriptRoot 'test_rh850_can_v4.ps1') -BootProject $boot -DeviceAdapter
    if($LASTEXITCODE){throw 'Actual v4/device adapter model failed.'}
    $objs=@()
    foreach($module in @('device_image','image_metadata','image_metadata_profile','boot_engine','flash_geometry')){
        & cl /nologo /TC /Za /W4 /WX @defines ('/I'+$common) (Join-Path $common ($module+'.c')) ('/Fo:'+ $module+'.obj') /c
        if($LASTEXITCODE){throw "Actual C90 module failed: $module"};$objs+=($module+'.obj')
    }
    $sources=@((Join-Path $repo 'tests\rh850_boot_tests.cpp'))
    foreach($s in @('host_link_protocol','host_link_client','can_host_client','f13_boot_profile','f13_boot_update','firmware_update_profile')){$sources+=Join-Path $core ($s+'.cpp')}
    & cl /nologo /std:c++17 /W4 /WX /EHsc @defines ('/I'+$common) ('/I'+$core) @sources @objs /Fe:rh850_boot_tests.exe
    if($LASTEXITCODE){throw 'RH850 model build failed.'}
    & .\rh850_boot_tests.exe $image
    if($LASTEXITCODE){throw 'RH850 model failed.'}
}finally{Pop-Location}
