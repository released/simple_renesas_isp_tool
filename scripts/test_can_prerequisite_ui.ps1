[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')][string]$Configuration='Release',
    [Parameter(Mandatory=$true)][string]$ClassicalImage,
    [Parameter(Mandatory=$true)][string]$FdImage,
    [string]$UartImage='',
    [string]$I2cImage='',
    [string]$XmodemImage='',
    [switch]$CapturePreview
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
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'can_prerequisite' -ShortPath
$output=$scope.Path
$runtime=if($Configuration -eq 'Debug'){@('/MTd','/D_DEBUG')}else{@('/MT','/DNDEBUG')}
$linkOptions=if($Configuration -eq 'Debug'){@('/DEBUG','/INCREMENTAL','/OPT:NOICF')}else{@('/LTCG')}
Push-Location -LiteralPath $output
try{
    $compileArgs=@('/nologo','/std:c++17','/W4','/WX','/EHsc','/D_UNICODE','/DUNICODE','/D_AFX_NO_MFC_CONTROLS_IN_DIALOGS')+
        $runtime+@((Join-Path $tool 'tests\can_prerequisite_ui_tests.cpp'))+$inputs+$resources+
        @('/Fe:prerequisite_fixture.exe','/link')+$linkOptions+
        @('/SUBSYSTEM:CONSOLE','setupapi.lib','cfgmgr32.lib')
    & cl @compileArgs
    if($LASTEXITCODE -ne 0){throw 'MFC prerequisite fixture build failed.'}
    & .\prerequisite_fixture.exe $ClassicalImage $FdImage $UartImage $I2cImage $XmodemImage
    if($LASTEXITCODE -ne 0){throw 'MFC prerequisite fixture failed.'}
}finally{
    Pop-Location
    try { if($CapturePreview) {
        $evidence=Join-Path $tool 'tmp\internal\verification'
        if((Test-Path -LiteralPath $evidence) -and
            ((Get-Item -LiteralPath $evidence -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Preview evidence reparse directory refused.'
        }
        [void][IO.Directory]::CreateDirectory($evidence)
        foreach($name in @('loaded_bin_valid.png','loaded_bin_wrong_route.png','serial_provenance_fixture.png')) {
            $capture=Join-Path $scope.Path $name
            if(Test-Path -LiteralPath $capture) {
                Copy-Item -LiteralPath $capture -Destination (Join-Path $evidence ($Configuration+'_'+$name))
            }
        }
    } } finally { Remove-OwnedTestWorkspace $scope }
}
Write-Host ('Owned UI fixture retired: '+$output)
