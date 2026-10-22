[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [ValidateSet('All','F13')][string]$Scope = 'All'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$tool=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $tool ('tmp\internal\tests\tb_'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $fixture | Out-Null
$names=@('RL78_F13_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_FD_app')
if($Scope -eq 'F13'){$names=@($names[0])}
$preserved=@{};$cases=@();$negative=0
function Require([bool]$Ok,[string]$Reason){if(!$Ok){throw $Reason}}
function Reject([scriptblock]$Action){
    $rejected=$false
    try{& $Action | Out-Null}catch{$rejected=$true}
    Require $rejected 'Invalid image or size was accepted.'
}
function Run-Generator([string]$Script,[string[]]$Options,[bool]$Pass){
    $ErrorActionPreference='Continue'
    $log=& powershell -NoProfile -ExecutionPolicy Bypass -File $Script @Options 2>&1
    $code=$LASTEXITCODE
    if(($code -eq 0) -ne $Pass){throw ('Unexpected generator result: '+($log -join "`n"))}
}
function Write-New([string]$Path,[byte[]]$Bytes){
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew)
    try{$stream.Write($Bytes,0,$Bytes.Length)}finally{$stream.Dispose()}
}
$body=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'device_test_bins.ps1'))
$embedded=[regex]::Match($body,"(?s)Add-Type -TypeDefinition @'\r?\n(.*?)\r?\n'@")
Require $embedded.Success 'Cannot find actual generator C# body.'
Add-Type -TypeDefinition $embedded.Groups[1].Value
Require ([DeviceFfTestBin]::Crc([Text.Encoding]::ASCII.GetBytes('123456789'),9) -eq [uint32]3421780262) 'CRC32 known answer failed.'
foreach($name in $names){
    $realApp=Join-Path $SourceRoot $name
    $realBoot=Join-Path $SourceRoot $name.Substring(0,$name.Length-4)
    $baseBin=Join-Path $realApp ('DefaultBuild\'+$name+'_update.bin')
    $preserved[$baseBin]=(Get-FileHash -LiteralPath $baseBin).Hash
    $parent=Join-Path $fixture ('case'+$cases.Count+' space '+[char]0x6e2c)
    $app=Join-Path $parent $name;$boot=Join-Path $parent $name.Substring(0,$name.Length-4)
    New-Item -ItemType Directory -Path (Join-Path $app 'DefaultBuild'),$boot | Out-Null
    foreach($file in @('generate_test_bins.ps1','generate_test_bins.bat','app_image_config.h')){
        Copy-Item -LiteralPath (Join-Path $realApp $file) -Destination (Join-Path $app $file)
    }
    Copy-Item -LiteralPath $baseBin -Destination (Join-Path $app ('DefaultBuild\'+$name+'_update.bin'))
    $reader=Join-Path $realBoot 'app_config.ps1'
    if(!(Test-Path -LiteralPath $reader)){$reader=Join-Path $realBoot 'tools\app_config.ps1'}
    Copy-Item -LiteralPath $reader -Destination (Join-Path $boot 'app_config.ps1')
    $script=Join-Path $app 'generate_test_bins.ps1'
    Run-Generator $script @() $true
    $batch=@(Get-ChildItem -LiteralPath (Join-Path $app 'test_bins') -Directory)[0].FullName
    $rows=Import-Csv -LiteralPath (Join-Path $batch 'manifest.csv')
    $expected=if($name -like '*F13*'){3}else{5}
    Require ($rows.Count -eq $expected) 'Wrong default size matrix.'
    $original=[IO.File]::ReadAllBytes($baseBin)
    $capacity=[DeviceFfTestBin]::Read32($original,0x120)
    Reject { [DeviceFfTestBin]::Build($original,$original.Length-4) };$negative++
    Reject { [DeviceFfTestBin]::Build($original,$capacity+4) };$negative++
    Reject { [DeviceFfTestBin]::Build($original,$original.Length+1) };$negative++
    Require ([Convert]::ToBase64String([DeviceFfTestBin]::Build($original,$original.Length)) -ceq
        [Convert]::ToBase64String($original)) 'Unchanged-length image differs.'
    foreach($offset in @(0,0x118,0x166,0x180)){
        $bad=[byte[]]$original.Clone();$bad[$offset]=$bad[$offset] -bxor 1
        [Array]::Copy([BitConverter]::GetBytes([DeviceFfTestBin]::Crc($bad,$bad.Length-4)),0,$bad,$bad.Length-4,4)
        Reject { [DeviceFfTestBin]::Validate($bad) };$negative++
    }
    $hashes=@{}
    foreach($row in $rows){
        $path=Join-Path $batch $row.file;$bytes=[IO.File]::ReadAllBytes($path)
        Require ($bytes.Length -eq [long]$row.input_bytes) 'Wrong manifest length.'
        Require ((Get-FileHash -LiteralPath $path).Hash -eq $row.sha256) 'Wrong manifest SHA.'
        for($i=0;$i -lt $original.Length-4;$i++){
            if($i -ge 0x118 -and $i -lt 0x11c){continue}
            Require ($original[$i] -eq $bytes[$i]) 'Executable/vector/metadata changed.'
        }
        for($i=$original.Length-4;$i -lt $bytes.Length-4;$i++){Require ($bytes[$i] -eq 255) 'Non-FF test padding.'}
        Require ([BitConverter]::ToUInt32($bytes,0x118) -eq $bytes.Length) 'Wrong encoded length.'
        $hashes[$path]=$row.sha256
    }
    # Exercise the real double-click entry with one supplied Enter key.
    $wrapper=Join-Path $app 'generate_test_bins.bat'
    $batchLog=& cmd /d /c ('echo. | "'+$wrapper+'"') 2>&1
    Require ($LASTEXITCODE -eq 0) ('Batch wrapper failed: '+($batchLog -join "`n"))
    Require (@(Get-ChildItem -LiteralPath (Join-Path $app 'test_bins') -Directory).Count -eq 2) 'Batch overwrite or missing second output.'
    foreach($path in $hashes.Keys){Require ((Get-FileHash -LiteralPath $path).Hash -eq $hashes[$path]) 'Existing test BIN overwritten.'}
    Run-Generator $script @('-FlashBudgetKiB','1') $false;$negative++
    Run-Generator $script @('-FlashBudgetKiB','0') $false;$negative++
    $bad=[byte[]]$original.Clone();$bad[512]=$bad[512] -bxor 1
    $badPath=Join-Path $app 'corrupt.bin';Write-New $badPath $bad
    Run-Generator $script @('-InputBin',$badPath) $false;$negative++
    $header=Join-Path $app 'app_image_config.h'
    $text=[IO.File]::ReadAllText($header)
    $stale=[regex]::Replace($text,'(?m)^(\s*#define\s+APP_IMAGE_VERSION\s+).+$','${1}(0x00000000UL)')
    [IO.File]::WriteAllText($header,$stale)
    Run-Generator $script @() $false;$negative++
    [IO.File]::WriteAllText($header,$text)
    Require (@(Get-ChildItem -LiteralPath (Join-Path $app 'test_bins') -Directory).Count -eq 2) 'Invalid input created outputs.'
    $outside=Join-Path $parent 'outside';$linkedApp=Join-Path $parent 'linked_app'
    New-Item -ItemType Directory -Path $outside,$linkedApp | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $linkedApp 'test_bins') -Target $outside | Out-Null
    Copy-Item -LiteralPath $header -Destination (Join-Path $linkedApp 'app_image_config.h')
    Run-Generator $script @('-ProjectRoot',$linkedApp,'-BootProject',$boot,'-InputBin',$baseBin) $false;$negative++
    Require (@(Get-ChildItem -LiteralPath $outside).Count -eq 0) 'Junction target modified.'
    $cases+=[pscustomobject]@{Name=$name;Boot=$realBoot;Batch=$batch;Rows=$rows}
}

$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Require (![string]::IsNullOrWhiteSpace($vs)) 'MSVC is required for maintainer native tests, not the user generator.'
$environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
Require ($LASTEXITCODE -eq 0) 'MSVC environment failed.'
foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
foreach($case in $cases){
    $native=Join-Path $fixture ($case.Name+'_native');New-Item -ItemType Directory -Path $native | Out-Null
    Push-Location -LiteralPath $native
    try{
        & cl /nologo /std:c++17 /W4 /WX /EHsc ('/I'+(Join-Path $tool 'src\core')) ('/I'+$case.Boot) `
            (Join-Path $tool 'tests\device_test_bins_validation.cpp') `
            (Join-Path $tool 'src\core\firmware_update_profile.cpp') `
            (Join-Path $case.Boot 'image_metadata.c') (Join-Path $case.Boot 'image_metadata_profile.c') /Fe:validate.exe
        Require ($LASTEXITCODE -eq 0) 'Native parser validation build failed.'
        $images=@($case.Rows | ForEach-Object {Join-Path $case.Batch $_.file})
        & (Join-Path $native 'validate.exe') @images
        Require ($LASTEXITCODE -eq 0) 'Actual Tool/Boot parser rejected generated test BIN.'
        if($case.Name -eq 'RL78_F13_Boot_loader_CAN_app'){
            & cl /nologo /TC /W4 /WX /D__near= /D__far= `
                ('/I'+(Join-Path $tool 'tests\f13_flash_mock')) ('/I'+$case.Boot) `
                ('/I'+(Join-Path $case.Boot 'FSL\lib')) `
                (Join-Path $tool 'tests\f13_flash_boundary_tests.c') `
                (Join-Path $case.Boot 'fsl_flash_interface.c') /Fe:flash_bounds.exe
            Require ($LASTEXITCODE -eq 0) 'Actual F13 Flash wrapper test build failed.'
            & (Join-Path $native 'flash_bounds.exe')
            Require ($LASTEXITCODE -eq 0) 'F13 Flash bounds test failed.'
        }
    }finally{Pop-Location}
}
foreach($path in $preserved.Keys){Require ((Get-FileHash -LiteralPath $path).Hash -eq $preserved[$path]) 'Formal App BIN changed.'}
Write-Host ('PASS: '+(($cases | ForEach-Object {$_.Rows.Count} | Measure-Object -Sum).Sum)+' FF images, '+$cases.Count+' batch wrappers, repeat/no-overwrite, '+$negative+' failure cases, portable paths, actual Tool/Boot parsers.')
Write-Host ('Fixture retained: '+$fixture)
