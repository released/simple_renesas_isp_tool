[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC development tools are required.'}
$environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'app_restart' -ShortPath
$output=$scope.Path
$pushed=$false
try {
    Push-Location -LiteralPath $output
    $pushed=$true
    & cl /nologo /std:c++17 /W4 /WX /EHsc (Join-Path $repo 'tests\app_restart_timing_tests.cpp') /Fe:app_restart_timing.exe
    if($LASTEXITCODE){throw 'Restart timing test build failed.'}
    & .\app_restart_timing.exe
    if($LASTEXITCODE){throw 'Restart timing test failed.'}
} finally {
    try {
        if($pushed){Pop-Location}
    } finally {
        Remove-OwnedTestWorkspace $scope
    }
}
Write-Host ('Owned test payload retired: '+$scope.Path)
