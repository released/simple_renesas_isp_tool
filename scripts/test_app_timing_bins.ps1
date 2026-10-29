param([Parameter(Mandatory=$true)][string]$DeviceRoot,[switch]$Native,[string[]]$ProjectName=@(),[switch]$PostHooks)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'device_app_catalog.ps1')
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
$apps=@(Get-DeviceTimingApps $DeviceRoot -ProjectName $ProjectName)
$fixture=Join-Path $repo ('tmp\internal\at_'+[guid]::NewGuid().ToString('N').Substring(0,8))
if(Test-Path -LiteralPath $fixture){throw 'Fresh fixture required.'}
[IO.Directory]::CreateDirectory($fixture)|Out-Null
$previousTemp=$env:TEMP;$previousTmp=$env:TMP
$env:TEMP=Join-Path $fixture 'temp';$env:TMP=$env:TEMP
[IO.Directory]::CreateDirectory($env:TEMP)|Out-Null
$passed=0;$images=0;$negative=0;$bytesRemoved=0L;$preserved=@{}
function Need([bool]$Ok,[string]$Why){if(!$Ok){throw $Why}}
function Reject-Image([scriptblock]$Action) {
    $rejected=$false;try{& $Action|Out-Null}catch{$rejected=$true}
    Need $rejected 'Invalid image accepted.'
}
function Run([string]$Script,[string[]]$Options,[bool]$Pass=$true) {
    $ErrorActionPreference='Continue'
    $lines=& powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $Script @Options 2>&1
    $code=$LASTEXITCODE
    if(($code -eq 0) -ne $Pass){throw ($Script+': '+($lines -join "`n"))}
}
function Seal([byte[]]$Bytes){[Array]::Copy([BitConverter]::GetBytes([DeviceFfTestBin]::Crc($Bytes,$Bytes.Length-4)),0,$Bytes,$Bytes.Length-4,4)}
try {
    $body=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'device_test_bins.ps1'))
    $code=[regex]::Match($body,"(?s)Add-Type -TypeDefinition @'\r?\n(.*?)\r?\n'@").Groups[1].Value
    Need (![string]::IsNullOrEmpty($code)) 'Actual generator C# missing.'
    Add-Type -TypeDefinition $code
    Need ([DeviceFfTestBin]::Crc([Text.Encoding]::ASCII.GetBytes('123456789'),9) -eq [uint32]3421780262) 'CRC known answer.'
    if($Native) {
        $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        Need (![string]::IsNullOrWhiteSpace($vs)) 'Existing MSVC required for maintainer native tests.'
        $environment=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
        Need ($LASTEXITCODE -eq 0) 'MSVC environment.'
        foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    }
    foreach($target in $apps) {
        $case=Join-Path $fixture ('case'+$passed)
        $app=Join-Path $case $target.Name
        [IO.Directory]::CreateDirectory((Join-Path $app 'DefaultBuild'))|Out-Null
        foreach($pair in @(@('device_test_bins.ps1','generate_test_bins.ps1'),@('device_test_bins.bat','generate_test_bins.bat'),@('device_test_bins_output.ps1','device_test_bins_output.ps1'))) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot $pair[0]) -Destination (Join-Path $app $pair[1])
        }
        Copy-Item -LiteralPath (Join-Path $target.Project 'app_image_config.h') -Destination $app
        Copy-Item -LiteralPath (Join-Path $target.Project ($target.Name+'.mtpj')) -Destination $app
        foreach($relative in @('app_config.ps1','tools\app_config.ps1','image_metadata_config.h',
            'isp_transport_config.h','can_boot_config.h','boot_app_handoff.h')) {
            $from=Join-Path $target.Project $relative
            if(Test-Path -LiteralPath $from) {
                $to=Join-Path $app $relative
                [IO.Directory]::CreateDirectory((Split-Path -Parent $to))|Out-Null
                Copy-Item -LiteralPath $from -Destination $to
            }
        }
        $source=Join-Path $target.Project ('DefaultBuild\'+$target.Name+'_update.bin')
        $preserved[$source]=(Get-FileHash -LiteralPath $source).Hash
        $bin=Join-Path $app ('DefaultBuild\'+$target.Name+'_update.bin')
        Copy-Item -LiteralPath $source -Destination $bin
        $original=[IO.File]::ReadAllBytes($bin)
        $script=Join-Path $app 'generate_test_bins.ps1'
        Run $script @('-Automatic')
        $auto=Join-Path $app 'test_bins\auto'
        $rows=@(Import-Csv -LiteralPath (Join-Path $auto 'manifest.csv'))
        $capacity=[DeviceFfTestBin]::Read32($original,0x120)
        foreach($length in @(($original.Length-4),($capacity+4),($original.Length+1))) {
            Reject-Image {[DeviceFfTestBin]::Build($original,$length)};$negative++
        }
        foreach($offset in @(0,0x118,0x166,0x180)) {
            $invalid=[byte[]]$original.Clone();$invalid[$offset]=$invalid[$offset] -bxor 1;Seal $invalid
            Reject-Image {[DeviceFfTestBin]::Validate($invalid)};$negative++
        }
        Need (($rows|Measure-Object -Property input_bytes -Maximum).Maximum -eq $capacity) 'Maximum capacity is missing.'
        $hashes=@{}
        foreach($row in $rows) {
            $path=Join-Path $auto $row.file
            $bytes=[IO.File]::ReadAllBytes($path)
            [DeviceFfTestBin]::Validate($bytes)
            Need ($bytes.Length -eq [int]$row.input_bytes) 'Manifest length.'
            Need ((Get-FileHash -LiteralPath $path).Hash -eq $row.sha256) 'Manifest SHA.'
            $expected=[DeviceFfTestBin]::Build($original,$bytes.Length)
            Need ([Convert]::ToBase64String($bytes) -ceq [Convert]::ToBase64String($expected)) 'FF/code/vector/metadata preservation.'
            $hashes[$row.file]=$row.sha256
        }
        Run $script @('-Automatic')
        Need (@(Get-ChildItem -LiteralPath (Join-Path $app 'test_bins') -Directory -Force).Count -eq 1) 'Repeated build accumulated batches.'
        foreach($file in $hashes.Keys){Need ((Get-FileHash -LiteralPath (Join-Path $auto $file)).Hash -eq $hashes[$file]) 'Idempotent output changed.'}
        if($Native) {
            $nativeDir=Join-Path $case 'native';[IO.Directory]::CreateDirectory($nativeDir)|Out-Null
            $metadata=Get-DeviceProjectSource $target.Project 'image_metadata.c'
            $profile=Get-DeviceProjectSource $target.Project 'image_metadata_profile.c'
            $includes=@('/I'+$target.Project)
            foreach($folder in @('hal\image','hal\device','hal\isp','hal\can','hal\flash')) {
                $include=Join-Path $target.Project $folder
                if(Test-Path -LiteralPath $include){$includes+=('/I'+$include)}
            }
            [xml]$targetXml=[IO.File]::ReadAllText((Join-Path $target.Project ($target.Name+'.mtpj')))
            $defines=@()
            if($targetXml.SelectSingleNode('//COptionD-0').InnerText -match '(?m)^RH850_CAN_FD\s*=\s*1\s*$'){$defines+='/DRH850_CAN_FD=1'}
            Push-Location -LiteralPath $nativeDir
            try {
                & cl /nologo /std:c++17 /W4 /WX /EHsc @defines @includes ('/I'+(Join-Path $repo 'src\core')) `
                    (Join-Path $repo 'tests\device_test_bins_validation.cpp') (Join-Path $repo 'src\core\firmware_update_profile.cpp') `
                    $metadata $profile /Fe:validate.exe
                Need ($LASTEXITCODE -eq 0) 'Actual Tool/Boot parser test compile.'
                & .\validate.exe @($rows|ForEach-Object {Join-Path $auto $_.file})
                Need ($LASTEXITCODE -eq 0) 'Actual Tool/Boot parser test.'
                if($target.Name -eq 'RL78_F13_Boot_loader_CAN_app') {
                    & cl /nologo /TC /W4 /WX /D__near= /D__far= ('/I'+(Join-Path $repo 'tests\f13_flash_mock')) @includes `
                        ('/I'+(Join-Path $target.Project 'FSL\lib')) (Join-Path $repo 'tests\f13_flash_boundary_tests.c') `
                        (Get-DeviceProjectSource $target.Project 'fsl_flash_interface.c') /Fe:flash_bounds.exe
                    Need ($LASTEXITCODE -eq 0) 'F13 Flash bounds fixture compile.'
                    & .\flash_bounds.exe
                    Need ($LASTEXITCODE -eq 0) 'F13 Flash bounds fixture.'
                }
            } finally {Pop-Location}
        }
        # Changed automatic output and unknown additions must survive refusal.
        $first=Join-Path $auto $rows[0].file
        $saved=[IO.File]::ReadAllBytes($first);$changed=[byte[]]$saved.Clone();$changed[512]=$changed[512] -bxor 1
        [IO.File]::WriteAllBytes($first,$changed)
        Run $script @('-Automatic') $false;$negative++
        Need ((Get-FileHash -LiteralPath $first).Hash -ne $rows[0].sha256) 'Modified output overwritten.'
        if($PostHooks) {
            [xml]$projectXml=[IO.File]::ReadAllText((Join-Path $target.Project ($target.Name+'.mtpj')))
            $command=$projectXml.SelectSingleNode('//PostBuildCommands-0').InnerText
            $match=[regex]::Match($command,'-File\s+"([^"]+)"')
            Need $match.Success 'Cannot locate actual post-build script.'
            $path=$match.Groups[1].Value.Replace('%ProjectDir%',$target.Project)
            if(![IO.Path]::IsPathRooted($path)){$path=Join-Path $target.Project $path}
            $hook=[regex]::Match([IO.File]::ReadAllText($path),'(?s)# APP_TIMING_BINS_BEGIN.*?# APP_TIMING_BINS_END').Value
            Need (![string]::IsNullOrEmpty($hook)) 'Actual post-package timing hook missing.'
            $wrapper=Join-Path $app 'post_failure.ps1'
            [IO.File]::WriteAllText($wrapper,('$ErrorActionPreference=''Stop'';$Kind=''app'';$isApp=$true;$Project=$PSScriptRoot'+"`n"+$hook))
            Run $wrapper @() $false;$negative++
            Need ((Get-FileHash -LiteralPath $first).Hash -ne $rows[0].sha256) 'Fail-fast hook changed protected fault image.'
        }
        [IO.File]::WriteAllBytes($first,$saved)
        $note=Join-Path $auto 'user-note.txt';[IO.File]::WriteAllText($note,'fixture-owned unknown addition')
        Run $script @('-Automatic') $false;$negative++
        Need (Test-Path -LiteralPath $note) 'Unknown addition removed.'
        [IO.File]::Delete($note)
        if($passed -eq 0) {
            # Ownership traversal, interrupted staging and junctions are refused.
            $ownedPath=Join-Path $auto '.owned.csv';$ownedBytes=[IO.File]::ReadAllBytes($ownedPath)
            $ownership=@(Import-Csv -LiteralPath $ownedPath);$ownership[0].file='..\outside.bin'
            try {
                $ownership|Export-Csv -LiteralPath $ownedPath -NoTypeInformation
                Run $script @('-Automatic') $false;$negative++
            } finally {[IO.File]::WriteAllBytes($ownedPath,$ownedBytes)}
            $stale=Join-Path (Split-Path -Parent $auto) '.new_1234abcd'
            [IO.Directory]::CreateDirectory($stale)|Out-Null
            try {Run $script @('-Automatic') $false;$negative++;Need (Test-Path -LiteralPath $stale) 'Interrupted staging removed.'}
            finally {[IO.Directory]::Delete($stale,$false)}
            $kept=Join-Path (Split-Path -Parent $auto) 'kept'
            $sentinel=Join-Path $case 'sentinel';[IO.Directory]::CreateDirectory($sentinel)|Out-Null
            $marker=Join-Path $sentinel 'protected.txt';[IO.File]::WriteAllText($marker,'fixture-owned protected target')
            $markerHash=(Get-FileHash -LiteralPath $marker).Hash
            [IO.Directory]::Move($auto,$kept)
            try {
                New-Item -ItemType Junction -Path $auto -Target $sentinel|Out-Null
                Run $script @('-Automatic') $false;$negative++
                Need ((Get-FileHash -LiteralPath $marker).Hash -ceq $markerHash) 'Junction target changed.'
            } finally {
                if(Test-Path -LiteralPath $auto){[IO.Directory]::Delete($auto,$false)}
                [IO.Directory]::Move($kept,$auto)
            }
        }
        $bad=[byte[]]$original.Clone();$bad[512]=$bad[512] -bxor 1;[IO.File]::WriteAllBytes($bin,$bad)
        Run $script @('-Automatic') $false;$negative++
        [IO.File]::WriteAllBytes($bin,$original)
        # A new valid release refreshes the one automatic set,not manual runs.
        $header=Join-Path $app 'app_image_config.h';$headerText=[IO.File]::ReadAllText($header)
        $version=[DeviceFfTestBin]::Read32($original,0x110)+1
        [IO.File]::WriteAllText($header,([regex]::Replace($headerText,'(?m)^(#define\s+APP_IMAGE_VERSION\s+).+$',('${1}(0x'+$version.ToString('X8')+'UL)'))))
        $new=[byte[]]$original.Clone();[Array]::Copy([BitConverter]::GetBytes([uint32]$version),0,$new,0x110,4);Seal $new
        [IO.File]::WriteAllBytes($bin,$new)
        Run $script @('-Automatic')
        Need (@(Get-ChildItem -LiteralPath (Join-Path $app 'test_bins') -Directory -Force).Count -eq 1) 'Replacement retained staging/old directory.'
        Need ((Import-Csv -LiteralPath (Join-Path $auto 'manifest.csv'))[0].app_version -eq ('0x{0:X8}' -f $version)) 'New release not reflected.'
        if($passed -eq 0) {
            $batchLog=& cmd /d /c ('echo. | "'+(Join-Path $app 'generate_test_bins.bat')+'"') 2>&1
            Need ($LASTEXITCODE -eq 0) ('Manual batch entry failed: '+($batchLog -join "`n"))
            $manual=@(Get-ChildItem -LiteralPath (Join-Path $app 'test_bins') -Directory|Where-Object {$_.Name -ne 'auto'})[0]
            $before=(Get-FileHash -LiteralPath (Join-Path $manual.FullName 'manifest.csv')).Hash
            Run $script @()
            Need ((Get-FileHash -LiteralPath (Join-Path $manual.FullName 'manifest.csv')).Hash -eq $before) 'Manual batch overwritten.'
            Need (@(Get-ChildItem -LiteralPath (Join-Path $app 'test_bins') -Directory).Count -eq 3) 'Manual batches not independent.'
        }
        $images+=$rows.Count;$passed++
        Write-Host ('PASS timing images: '+$target.Name+' / '+$rows.Count+' sizes / capacity '+($capacity/1024)+' KiB')
    }
    foreach($path in $preserved.Keys){Need ((Get-FileHash -LiteralPath $path).Hash -eq $preserved[$path]) 'Original packaged BIN changed.'}
    Write-Host ('PASS '+$passed+' Apps, '+$images+' images, '+$negative+' failure cases; actual Tool/Boot parsers='+[bool]$Native)
} finally {
    $env:TEMP=$previousTemp;$env:TMP=$previousTmp
    # The fresh GUID fixture contains only this runner's copies/compiler outputs.
    $root=[IO.Path]::GetFullPath($repo).TrimEnd('\')
    if(!$fixture.StartsWith($root+'\tmp\internal\at_',[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture boundary mismatch.'}
    $cursor=$fixture
    while($cursor) {
        if((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Fixture reparse ancestor prevents cleanup.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
    $pending=New-Object 'Collections.Generic.Stack[string]';$pending.Push($fixture);$items=@()
    while($pending.Count) {
        foreach($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if(!$item.FullName.StartsWith($fixture+'\',[StringComparison]::OrdinalIgnoreCase) -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Fixture path/reparse point prevents cleanup.'}
            $items+=$item;if($item.PSIsContainer){$pending.Push($item.FullName)}
        }
    }
    $owned=@($items|Where-Object {!$_.PSIsContainer}|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Bytes=$_.Length;Hash=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($entry in $owned){if((Get-FileHash -LiteralPath $entry.Path).Hash -cne $entry.Hash){throw 'Fixture changed during cleanup.'}}
    foreach($entry in $owned){[IO.File]::Delete($entry.Path);$bytesRemoved+=$entry.Bytes}
    foreach($dir in @($items|Where-Object {$_.PSIsContainer}|Sort-Object {$_.FullName.Length} -Descending)){[IO.Directory]::Delete($dir.FullName,$false)}
    [IO.Directory]::Delete($fixture,$false)
    Write-Host ('CLEANUP: '+$owned.Count+' owned fixture files / '+$bytesRemoved+' bytes removed; retained payload 0 bytes.')
}
