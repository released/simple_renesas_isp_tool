[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$DeviceRoot,
    [ValidateSet('Release','Debug')][string]$Configuration='Release',
    [switch]$NativeOnly,
    [switch]$UiOnly
)
$ErrorActionPreference='Stop'
if($NativeOnly -and $UiOnly){throw 'Select either NativeOnly or UiOnly.'}
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'review_followup' -ShortPath
$passed=$false; $output=@(); $retired=0
try {
    $vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs){throw 'Existing MSVC/MFC tools are required.'}
    $environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    Push-Location -LiteralPath $scope.Path
    try {
        $core=Join-Path $root 'src\core'
        if(!$UiOnly) {
        $sources=@('firmware_update_profile','f13_boot_profile','f13_boot_update','can_host_client','host_link_client','host_link_protocol')|ForEach-Object{Join-Path $core ($_+'.cpp')}
        & cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 ('/I'+$core) (Join-Path $root 'tests\review_followup_core_tests.cpp') @sources /Fe:followup_core.exe
        if($LASTEXITCODE){throw 'Follow-up core build failed.'}
        $names=@('RL78_F13_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_FD_app',
            'Sample_Project_RH850_S1_Boot_loader_CAN_app','Sample_Project_RH850_S1_Boot_loader_CAN_FD_app',
            'RL78_F13_ISP_Boot_loader_UART_app','RL78_F24_ISP_Boot_loader_I2C_app','RL78_G16_ISP_Boot_loader_Xmodem_app')
        $images=@($names|ForEach-Object{Join-Path $DeviceRoot ($_+'\DefaultBuild\'+$_+'_update.bin')})
        foreach($image in $images){if(!(Test-Path -LiteralPath $image)){throw ('Missing explicit image: '+$image)}}
        $output+=@(& .\followup_core.exe @images)
        if($LASTEXITCODE){throw 'Follow-up image/report checks failed.'}
        $coreObjects=@('firmware_update_profile','f13_boot_profile','f13_boot_update','can_host_client','host_link_client','host_link_protocol')|ForEach-Object{Join-Path $scope.Path ($_+'.obj')}
        & cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 /Gy /wd4505 ('/I'+$core) (Join-Path $root 'tests\can_preflight_regression_tests.cpp') @coreObjects /Fe:can_preflight.exe /link /OPT:REF
        if($LASTEXITCODE){throw 'Focused existing CAN fixture build failed.'}
        $output+=@(& .\can_preflight.exe)
        if($LASTEXITCODE){throw 'Existing CAN profile/update regression failed.'}
        & cl /nologo /std:c++17 /EHsc /W4 /WX /MT /O2 (Join-Path $root 'tests\application_instance_tests.cpp') /Fe:instance_fixture.exe user32.lib
        if($LASTEXITCODE){throw 'Application ownership fixture build failed.'}
        Copy-Item -LiteralPath '.\instance_fixture.exe' -Destination '.\renamed_fixture.exe'
        $output+=@(& .\instance_fixture.exe (Join-Path $scope.Path 'renamed_fixture.exe'))
        if($LASTEXITCODE){throw 'Application ownership checks failed.'}
        }
        if(!$NativeOnly) {
            $objects=Join-Path $root ('tmp\internal\build\obj\x64\'+$Configuration)
            $inputs=@(Get-ChildItem -LiteralPath $objects -Filter '*.obj' | Where-Object {$_.Name -notin @('app.obj','main.obj')} | ForEach-Object FullName)
            $resources=@(Get-ChildItem -LiteralPath $objects -Filter '*.res'|ForEach-Object FullName)
            if($inputs.Count -lt 30){throw 'Build the selected product configuration first.'}
            $runtime=if($Configuration -eq 'Debug'){@('/MTd','/D_DEBUG')}else{@('/MT','/DNDEBUG')}
            $link=if($Configuration -eq 'Debug'){@('/DEBUG','/INCREMENTAL','/OPT:NOICF')}else{@('/LTCG')}
            if($Configuration -eq 'Debug'){Copy-Item -LiteralPath (Join-Path $objects 'vc143.pdb') -Destination (Join-Path $scope.Path 'vc143.pdb')}
            $args=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/D_UNICODE','/DUNICODE','/D_AFX_NO_MFC_CONTROLS_IN_DIALOGS')+$runtime+
                @((Join-Path $root 'tests\review_followup_ui_tests.cpp'))+$inputs+$resources+
                @('/Fe:followup_ui.exe','/link')+$link+@('/SUBSYSTEM:CONSOLE','setupapi.lib','cfgmgr32.lib','comctl32.lib')
            & cl @args
            if($LASTEXITCODE){throw 'Follow-up MFC fixture build failed.'}
            $uiExe=Join-Path $scope.Path 'followup_ui.exe'
            $stdout=Join-Path $scope.Path 'ui.stdout'
            $stderr=Join-Path $scope.Path 'ui.stderr'
            $child=Start-Process -FilePath $uiExe -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
            $null=$child.Handle
            try {
                if(!$child.WaitForExit(30000)) {
                    if($child.Path -cne $uiExe){throw 'Owned UI process identity mismatch.'}
                    Stop-Process -Id $child.Id -ErrorAction Stop
                    $child.WaitForExit()
                    throw 'Owned no-COM UI fixture exceeded its 30-second bound.'
                }
                $child.WaitForExit()
                $child.Refresh()
                $output+=[IO.File]::ReadAllLines($stdout)
                Get-Content -LiteralPath $stderr|ForEach-Object{Write-Host $_}
                if($null -eq $child.ExitCode -or $child.ExitCode -ne 0 -or
                    !([IO.File]::ReadAllLines($stdout)|Where-Object{$_ -match '^Follow-up MFC: \d+ checks, 0 failures;'})) {
                    throw 'Follow-up MFC checks failed.'
                }
            } finally {$child.Dispose()}
        }
        $passed=$true
    } finally {Pop-Location}
} finally {
    $retired=(Get-ChildItem -LiteralPath $scope.Path -Recurse -File -Force|Measure-Object Length -Sum).Sum
    Remove-OwnedTestWorkspace $scope
    $record=[ordered]@{Passed=$passed;Configuration=$Configuration;NativeOnly=[bool]$NativeOnly;UiOnly=[bool]$UiOnly;Output=$output;RetiredBytes=$retired;FixtureRemoved=!(Test-Path -LiteralPath $scope.Path);Hardware=$false}
    $path=Join-Path $root ('tmp\internal\verification\review_followup_'+$Configuration+'_result.json')
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $path))
    $record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $path -Encoding UTF8
    $output|ForEach-Object{Write-Host $_}
}
