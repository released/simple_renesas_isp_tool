param([Parameter(Mandatory=$true)][string]$Project)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'f13_transfer_filter' -ShortPath
$results=@();$passed=$false;$bytes=0
try {
    $projectPath=(Resolve-Path -LiteralPath $Project).Path
    $name=Split-Path -Leaf $projectPath
    $fixture=Join-Path $scope.Path $name
    $files=@('tools\verify_transfer_filter.ps1','can_transfer_config.h','hal\can\RSCANlite.c','hal\can\RSCANlite.h',
        ('DefaultBuild\'+$name+'.map'),('DefaultBuild\'+$name+'.mot'))
    $original=@{}
    foreach($file in $files){
        $original[$file]=[IO.File]::ReadAllBytes((Join-Path $projectPath $file))
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent (Join-Path $fixture $file)))
    }
    function Restore-Fixture {
        foreach($file in $files){[IO.File]::WriteAllBytes((Join-Path $fixture $file),$original[$file])}
    }
    function Run-Case([string]$Name,[bool]$ExpectSuccess,[string]$Expected) {
        $previous=$ErrorActionPreference
        try {
            $ErrorActionPreference='Continue'
            $output=& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $fixture 'tools\verify_transfer_filter.ps1') -ProjectRoot $fixture -CheckImage 2>&1
            $exitCode=$LASTEXITCODE
        } finally {$ErrorActionPreference=$previous}
        $text=($output|ForEach-Object{[string]$_}) -join "`n"
        if(($exitCode -eq 0) -ne $ExpectSuccess -or !$text.Contains($Expected)){throw ('Unexpected verifier result: '+$Name+' '+$text)}
        $script:results+=@{Case=$Name;Passed=$true;ExpectedSuccess=$ExpectSuccess}
    }
    $mapFile='DefaultBuild\'+$name+'.map'
    $motFile='DefaultBuild\'+$name+'.mot'
    $map=[Text.Encoding]::UTF8.GetString($original[$mapFile])
    $match=[regex]::Match($map,'(?m)^\s*_g_rxrule_table\s*\r?\n\s*([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]+)\s+data\b')
    if(!$match.Success){throw 'Actual map symbol missing.'}
    $table=[Convert]::ToUInt32($match.Groups[1].Value,16)
    function Change-Byte([uint32]$Address,[byte]$Value,[bool]$RepairChecksum=$true) {
        $path=Join-Path $fixture $motFile
        $lines=[IO.File]::ReadAllLines($path)
        $changed=$false
        for($line=0;$line -lt $lines.Length;$line++){
            if($lines[$line] -notmatch '^S([123])([0-9A-Fa-f]+)$'){continue}
            $kind=[int]$Matches[1];$text=$Matches[2];$addressBytes=$kind+1
            $record=New-Object byte[] ($text.Length/2)
            for($i=0;$i -lt $record.Length;$i++){$record[$i]=[Convert]::ToByte($text.Substring($i*2,2),16)}
            [uint32]$base=0
            for($i=1;$i -le $addressBytes;$i++){$base=($base -shl 8) -bor $record[$i]}
            $count=$record[0]-$addressBytes-1
            if($Address -lt $base -or $Address -ge ($base+$count)){continue}
            $record[$addressBytes+1+$Address-$base]=$Value
            if($RepairChecksum){
                $sum=0
                for($i=0;$i -lt $record.Length-1;$i++){$sum+=$record[$i]}
                $record[$record.Length-1]=[byte](255-($sum -band 255))
            }
            $lines[$line]='S'+$kind+([BitConverter]::ToString($record)).Replace('-','')
            $changed=$true;break
        }
        if(!$changed){throw 'Mutation address missing from actual MOT.'}
        [IO.File]::WriteAllLines($path,$lines,[Text.Encoding]::ASCII)
    }
    Restore-Fixture
    Run-Case 'actual target map/MOT' $true 'PASS linked Transfer MOT'
    foreach($fault in @(@('Boot ID',0,0),@('wide mask',4,0),@('missing IDE/RTR mask',7,0),@('wrong FIFO',8,2))){
        Restore-Fixture;Change-Byte ([uint32]($table+$fault[1])) ([byte]$fault[2])
        Run-Case $fault[0] $false 'Linked RX filter mismatch'
    }
    Restore-Fixture
    Change-Byte $table 0 $false
    Run-Case 'invalid S-record checksum' $false 'Invalid S-record length/checksum'
    Restore-Fixture
    $badMap=$map.Remove($match.Groups[2].Index,$match.Groups[2].Length).Insert($match.Groups[2].Index,'18')
    [IO.File]::WriteAllText((Join-Path $fixture $mapFile),$badMap)
    Run-Case 'two linked Boot rules' $false 'exactly six16-bit words'
    Restore-Fixture
    $configPath=Join-Path $fixture 'can_transfer_config.h'
    [IO.File]::WriteAllText($configPath,([IO.File]::ReadAllText($configPath).Replace('(0x710U)','(0x712U)')))
    Run-Case 'config changed without rebuilding' $false 'Linked RX filter mismatch'
    Restore-Fixture
    $rulePath=Join-Path $fixture 'hal\can\RSCANlite.c'
    [IO.File]::WriteAllText($rulePath,([IO.File]::ReadAllText($rulePath).Replace('"can_transfer_config.h"','"can_boot_config.h"')))
    Run-Case 'Boot source copied into Transfer' $false 'never Boot configuration'
    $passed=$true
} finally {
    $bytes=(Get-ChildItem -LiteralPath $scope.Path -Recurse -File -Force|Measure-Object Length -Sum).Sum
    Remove-OwnedTestWorkspace $scope
    $record=@{Passed=$passed;Cases=$results;RetiredBytes=$bytes;FixtureRemoved=!(Test-Path -LiteralPath $scope.Path);Hardware=$false}
    $output=Join-Path $root 'tmp\internal\verification\f13_transfer_filter_result.json'
    $record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $output -Encoding UTF8
    Write-Host ('Transfer filter verifier: '+$results.Count+' cases; passed='+$passed+'; owned fixture removed='+!(Test-Path -LiteralPath $scope.Path))
}
