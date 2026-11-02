[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')][string]$Configuration='Release',
    [switch]$UpdateDelivery
)
$ErrorActionPreference='Stop'
if($UpdateDelivery -and $Configuration -ne 'Release'){throw 'Only verified Release may replace the delivery EXE.'}
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'misc_host_ui' -ShortPath
$evidence=Join-Path $root ('tmp\internal\verification\misc_host_ui_'+$Configuration)
[void][IO.Directory]::CreateDirectory($evidence)
$passed=$false; $output=@(); $bytes=0
try {
    $vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs){throw 'Existing MSVC/MFC tools are required.'}
    $environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $product=Join-Path $scope.Path 'product'
    $objects=Join-Path $root ('tmp\internal\build\obj\x64\'+$Configuration)
    & (Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe') (Join-Path $root 'FirmwareUpgradeTool.sln') /t:Build /m /v:minimal /nologo "/p:Configuration=$Configuration" /p:Platform=x64 ('/p:OutDir='+$product+'\') *> (Join-Path $evidence 'build.log')
    if($LASTEXITCODE){throw ('Product build failed; inspect '+(Join-Path $evidence 'build.log'))}
    Push-Location -LiteralPath $scope.Path
    try {
        $inputs=@(Get-ChildItem -LiteralPath $objects -Filter '*.obj'|Where-Object{$_.Name -notin @('app.obj','main.obj')}|ForEach-Object FullName)
        $resources=@(Get-ChildItem -LiteralPath $objects -Filter '*.res'|ForEach-Object FullName)
        if($inputs.Count -lt 30){throw 'Production object set is incomplete.'}
        $runtime=if($Configuration -eq 'Debug'){@('/MTd','/D_DEBUG')}else{@('/MT','/DNDEBUG')}
        $link=if($Configuration -eq 'Debug'){@('/DEBUG','/INCREMENTAL','/OPT:NOICF')}else{@('/LTCG')}
        if($Configuration -eq 'Debug'){Copy-Item -LiteralPath (Join-Path $objects 'vc143.pdb') -Destination 'vc143.pdb'}
        $args=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/D_UNICODE','/DUNICODE','/D_AFX_NO_MFC_CONTROLS_IN_DIALOGS')+$runtime+
            @(('/I'+(Join-Path $root 'src\core')),(Join-Path $root 'tests\misc_host_ui_tests.cpp'),
                (Join-Path $root 'tests\serial_port_catalog_tests.cpp'))+$inputs+$resources+@('/Fe:misc_host_ui.exe','/link')+$link+
            @('/SUBSYSTEM:CONSOLE','setupapi.lib','cfgmgr32.lib','comctl32.lib')
        & cl @args *> (Join-Path $evidence 'fixture_build.log')
        if($LASTEXITCODE){throw ('MFC fixture build failed; inspect '+(Join-Path $evidence 'fixture_build.log'))}
        $exe=Join-Path $scope.Path 'misc_host_ui.exe'
        $image=Join-Path $root 'firmware\RL78_F24_Bridge_app\DefaultBuild\RL78_F24_Bridge_app_bridge_update.bin'
        $stdout=Join-Path $scope.Path 'ui.stdout'; $stderr=Join-Path $scope.Path 'ui.stderr'
        $child=Start-Process -FilePath $exe -ArgumentList ('"'+$image+'"') -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $null=$child.Handle
        try {
            if(!$child.WaitForExit(30000)){
                if($child.Path -cne $exe){throw 'Owned test process identity differs.'}
                Stop-Process -Id $child.Id
                $child.WaitForExit()
                throw 'Owned no-COM fixture exceeded its 30-second bound.'
            }
            $child.WaitForExit(); $child.Refresh()
            $output=@([IO.File]::ReadAllLines($stdout))+@([IO.File]::ReadAllLines($stderr))
            $output|Set-Content -LiteralPath (Join-Path $evidence 'checks.log') -Encoding UTF8
            if($null -eq $child.ExitCode -or $child.ExitCode -ne 0 -or !($output|Where-Object{$_ -match '^Misc Host UI: \d+ checks, 0 failures;'})){
                throw ('Misc Host UI checks failed; inspect '+(Join-Path $evidence 'checks.log'))
            }
        } finally {$child.Dispose()}
        foreach($name in @('misc_boot_no_app.png','misc_app_running.png')){
            Copy-Item -LiteralPath (Join-Path $scope.Path $name) -Destination (Join-Path $evidence $name)
        }
        if($UpdateDelivery){
            Copy-Item -LiteralPath (Join-Path $product 'FirmwareUpgradeTool.exe') -Destination (Join-Path $root 'build\FirmwareUpgradeTool.exe')
        }
        $passed=$true
    } finally {Pop-Location}
} finally {
    $bytes=(Get-ChildItem -LiteralPath $scope.Path -Recurse -File -Force|Measure-Object Length -Sum).Sum
    Remove-OwnedTestWorkspace $scope
    @{Passed=$passed;Configuration=$Configuration;Output=$output;RetiredBytes=$bytes;FixtureRemoved=!(Test-Path -LiteralPath $scope.Path);Hardware=$false;DeliveryUpdated=($passed -and [bool]$UpdateDelivery)}|
        ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $evidence 'result.json') -Encoding UTF8
    $output|ForEach-Object{Write-Host $_}
}
