[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')][string]$Configuration='Release',
    [Parameter(Mandatory=$true)][string]$ClassicalImage,
    [Parameter(Mandatory=$true)][string]$FdImage
)
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Native UI fixture requires existing MSVC/MFC development tools.'}
$environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE -ne 0){throw 'MSVC environment failed.'}
foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$objects=Join-Path $tool ('tmp\internal\build\obj\x64\'+$Configuration)
$inputs=@(Get-ChildItem -LiteralPath $objects -Filter '*.obj' | Where-Object {$_.Name -notin @('app.obj','main.obj')} | ForEach-Object {$_.FullName})
if($inputs.Count -lt 30){throw 'Build the requested product configuration first.'}
$resources=@(Get-ChildItem -LiteralPath $objects -Filter '*.res' | ForEach-Object {$_.FullName})
$output=Join-Path $tool ('tmp\internal\tests\can_prerequisite_'+$Configuration+'_'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$runtime=if($Configuration -eq 'Debug'){@('/MTd','/D_DEBUG')}else{@('/MT','/DNDEBUG')}
$linkOptions=if($Configuration -eq 'Debug'){@('/DEBUG','/INCREMENTAL','/OPT:NOICF')}else{@('/LTCG')}
Push-Location -LiteralPath $output
try{
    $compileArgs=@('/nologo','/std:c++17','/W4','/WX','/EHsc','/D_UNICODE','/DUNICODE')+
        $runtime+@((Join-Path $tool 'tests\can_prerequisite_ui_tests.cpp'))+$inputs+$resources+
        @('/Fe:prerequisite_fixture.exe','/link')+$linkOptions+
        @('/SUBSYSTEM:CONSOLE','setupapi.lib','cfgmgr32.lib')
    & cl @compileArgs
    if($LASTEXITCODE -ne 0){throw 'MFC prerequisite fixture build failed.'}
    & .\prerequisite_fixture.exe $ClassicalImage $FdImage
    if($LASTEXITCODE -ne 0){throw 'MFC prerequisite fixture failed.'}
}finally{Pop-Location}
Write-Host ('Fixture retained: '+$output)
