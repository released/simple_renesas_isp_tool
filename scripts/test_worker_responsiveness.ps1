param([ValidateRange(0,180)][int]$PreviewSeconds=0)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'worker_ui' -ShortPath
$success=$false
$output=@()
try {
    $vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs) {throw 'Existing MSVC/MFC development tools are required.'}
    $environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE) {throw 'MSVC environment failed.'}
    foreach($line in $environment) {
        if($line -match '^([^=]+)=(.*)$') {[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
    }
    $objects=Join-Path $repo 'tmp\internal\build\obj\x64\Release'
    $inputs=@(Get-ChildItem -LiteralPath $objects -Filter '*.obj' |
        Where-Object {$_.Name -notin @('app.obj','main.obj')} | ForEach-Object FullName)
    $resources=@(Get-ChildItem -LiteralPath $objects -Filter '*.res' | ForEach-Object FullName)
    if($inputs.Count -lt 30) {throw 'Build Release|x64 before running the actual-frame fixture.'}
    Push-Location -LiteralPath $scope.Path
    try {
        & cl /nologo /std:c++17 /W4 /WX /EHsc /MT /DNDEBUG /D_UNICODE /DUNICODE /D_AFX_NO_MFC_CONTROLS_IN_DIALOGS `
            (Join-Path $repo 'tests\worker_responsiveness_tests.cpp') (Join-Path $repo 'tests\log_view_tests.cpp') `
            @inputs @resources /Fe:worker_ui.exe /link /LTCG /SUBSYSTEM:CONSOLE setupapi.lib cfgmgr32.lib comctl32.lib
        if($LASTEXITCODE) {throw 'Actual-frame fixture build failed.'}
        $output=@(& .\worker_ui.exe $PreviewSeconds)
        $exit=$LASTEXITCODE
        $output | ForEach-Object {Write-Host $_}
        if($exit) {throw 'Actual-frame fixture failed.'}
        & cl /nologo /std:c++17 /W4 /WX /EHsc /MT (Join-Path $repo 'tests\app_restart_timing_tests.cpp') /Fe:restart_timing.exe
        if($LASTEXITCODE) {throw 'Existing restart scheduling test build failed.'}
        & .\restart_timing.exe
        if($LASTEXITCODE) {throw 'Existing restart scheduling regression failed.'}
        $success=$true
    } finally {Pop-Location}
} finally {
    Remove-OwnedTestWorkspace $scope
    $report=[ordered]@{Result=$(if($success){'PASS'}else{'FAIL'});Output=$output;FixtureRemoved=!(Test-Path -LiteralPath $scope.Path)
        Hardware=$false;PreviewSeconds=$PreviewSeconds;Utc=[DateTime]::UtcNow.ToString('o')}
    $path=Join-Path $repo 'tmp\internal\verification\worker_ui_result.json'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $path))
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
}
