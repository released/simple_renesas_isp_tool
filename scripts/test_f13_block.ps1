[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BootProject,
    [Parameter(Mandatory=$true)][string]$Image
)
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
if(!(Test-Path -LiteralPath $Image -PathType Leaf)){throw 'Input image is missing.'}
$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Native maintainer tests require MSVC.'}
$environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE -ne 0){throw 'MSVC environment failed.'}
foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$output=Join-Path $tool ('tmp\internal\tests\f13_block_'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$sources=@('tests\can_block_tests.cpp','src\core\can_host_client.cpp',
    'src\core\f13_boot_profile.cpp','src\core\f13_boot_update.cpp',
    'src\core\firmware_update_profile.cpp','src\core\host_link_client.cpp',
    'src\core\host_link_protocol.cpp') | ForEach-Object {Join-Path $tool $_}
$sources+=Join-Path $BootProject 'image_metadata.c'
$sources+=Join-Path $BootProject 'image_metadata_profile.c'
Push-Location -LiteralPath $output
try{
    & cl /nologo /std:c++17 /W4 /WX /EHsc /DF13_BLOCK_TEST_STANDALONE `
        ('/I'+$BootProject) ('/I'+(Join-Path $tool 'src\core')) @sources /Fe:f13_block.exe
    if($LASTEXITCODE -ne 0){throw 'F13 block model compilation failed.'}
    & .\f13_block.exe $Image
    if($LASTEXITCODE -ne 0){throw 'F13 block update model failed.'}
}finally{Pop-Location}
Write-Host ('Fixture retained: '+$output)
