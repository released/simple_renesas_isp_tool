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
$output=Join-Path $repo ('tmp\internal\tests\app_restart_'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
Push-Location -LiteralPath $output
try {
    & cl /nologo /std:c++17 /W4 /WX /EHsc (Join-Path $repo 'tests\app_restart_timing_tests.cpp') /Fe:app_restart_timing.exe
    if($LASTEXITCODE){throw 'Restart timing test build failed.'}
    & .\app_restart_timing.exe
    if($LASTEXITCODE){throw 'Restart timing test failed.'}
} finally {Pop-Location}
Write-Host ('Fixture retained: '+$output)
