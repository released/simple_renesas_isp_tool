param([Parameter(Mandatory=$true)][string]$DeviceImage)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'review_core' -ShortPath
$success=$false; $output=@()
try {
    $vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs) { throw 'Installed MSVC is required.' }
    $environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE) { throw 'MSVC environment failed.' }
    foreach($line in $environment) { if($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
    Push-Location -LiteralPath $scope.Path
    try {
        & cl /nologo /c /TC /W4 /WX /MT (Join-Path $repo 'firmware\common\bridge_image_metadata.c') /Fo:bridge_metadata.obj
        if($LASTEXITCODE) { throw 'Metadata dependency build failed.' }
        $sources=@('tests/review_core_tests.cpp','tests/text_file_fault_tests.cpp','tests/serial_port_io_tests.cpp','tests/tool_experience_tests.cpp')
        foreach($name in @('host_link_protocol','host_link_client','can_host_client','bridge_port_catalog','firmware_update_profile','bridge_update_profile','g16_i2c_image','image_analysis')) {
            $sources+='src/core/'+$name+'.cpp'
        }
        $sources=@($sources | ForEach-Object {Join-Path $repo $_})
        & cl /nologo /std:c++17 /W4 /WX /O2 /EHsc /MT /I (Join-Path $repo 'src\core') @sources bridge_metadata.obj /Fe:review_core.exe
        if($LASTEXITCODE) { throw 'Review core build failed.' }
        $output=@(& .\review_core.exe $DeviceImage)
        $exit=$LASTEXITCODE; $output | ForEach-Object {Write-Host $_}
        if($exit) { throw 'Review core regression failed.' }
        $success=$true
    } finally { Pop-Location }
} finally {
    $bytes=(Get-ChildItem -LiteralPath $scope.Path -Recurse -File -Force | Measure-Object Length -Sum).Sum
    Remove-OwnedTestWorkspace $scope
    $record=@{Passed=$success;Output=$output;RetiredBytes=$bytes;FixtureRemoved=!(Test-Path -LiteralPath $scope.Path);Hardware=$false}
    $path=Join-Path $repo 'tmp\internal\verification\review_core_result.json'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $path))
    $record|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $path -Encoding UTF8
}
