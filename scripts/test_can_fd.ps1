param([Parameter(Mandatory=$true)][string]$DeviceBoot,[Parameter(Mandatory=$true)][string]$DeviceApp)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$boot=(Resolve-Path -LiteralPath $DeviceBoot).Path
$app=(Resolve-Path -LiteralPath $DeviceApp).Path
$image=Join-Path $app 'DefaultBuild\RL78_F24_Boot_loader_CAN_FD_app_update.bin'
if(!(Test-Path -LiteralPath $image)){throw 'Clean-build the selected FD App first.'}
$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC developer tools are required for native models.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$testScope=New-OwnedTestWorkspace 'can_fd'
$out=$testScope.Path
try {
$core=Join-Path $repo 'src\core'
$sources=@((Join-Path $repo 'tests\can_fd_upgrade_tests.cpp'),(Join-Path $boot 'image_metadata.c'),
    (Join-Path $boot 'image_metadata_profile.c'))
foreach($name in @('host_link_protocol','host_link_client','can_host_client','f13_boot_profile','f13_boot_update','firmware_update_profile')){
    $sources+=Join-Path $core ($name+'.cpp')
}
Push-Location -LiteralPath $out
try{
    & cl /nologo /std:c++17 /W4 /WX /EHsc ('/I'+$boot) ('/I'+$core) @sources /Fe:can_fd_upgrade_tests.exe
    if($LASTEXITCODE){throw 'FD native model build failed.'}
    & (Join-Path $out 'can_fd_upgrade_tests.exe') $image
    if($LASTEXITCODE){throw 'FD native model failed.'}
}finally{Pop-Location}

} finally { Remove-OwnedTestWorkspace $testScope }
