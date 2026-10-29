param([Parameter(Mandatory=$true)][string]$DeviceRoot,[string[]]$ProjectName=@())
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'device_app_catalog.ps1')
. (Join-Path $PSScriptRoot 'device_test_bins_output.ps1')
$repo=Split-Path -Parent $PSScriptRoot
function Assert-PlainBuildPath([string]$Path) {
    $cursor=[IO.Path]::GetFullPath($Path)
    while($cursor) {
        if((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {throw 'Build verification refuses reparse paths.'}
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
}
$apps=@(Get-DeviceTimingApps $DeviceRoot -ProjectName $ProjectName)
$compiler=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\CS+\CC\CubeSuite+.exe'
if(!(Test-Path -LiteralPath $compiler)){throw 'Existing CS+ compiler is required.'}
$run=Join-Path $repo ('tmp\internal\verification\app_timing_builds_'+[guid]::NewGuid().ToString('N').Substring(0,8))
Assert-PlainBuildPath $run
if(Test-Path -LiteralPath $run){throw 'Fresh evidence directory required.'}
[IO.Directory]::CreateDirectory($run)|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
$env:TEMP=Join-Path $run 'temp';$env:TMP=$env:TEMP;[IO.Directory]::CreateDirectory($env:TEMP)|Out-Null
$results=@();$success=$false
function Clean-AppObjects($App,[DateTime]$Started,[string]$ProjectFile) {
    Assert-PlainBuildPath $App.Project
    $output=Join-Path $App.Project 'DefaultBuild'
    Assert-PlainBuildPath $output
    [xml]$xml=[IO.File]::ReadAllText($ProjectFile)
    $expected=@{}
    foreach($node in $xml.SelectNodes('//Instance[Type="File"]')) {
        if([IO.Path]::GetExtension($node.Name) -in @('.c','.asm')){$expected[([IO.Path]::GetFileNameWithoutExtension($node.Name)+'.obj')]=$true}
    }
    $objects=@(Get-ChildItem -LiteralPath $output -File -Filter '*.obj'|
        Where-Object {$expected.ContainsKey($_.Name) -and $_.LastWriteTimeUtc -ge $Started.AddSeconds(-2)})
    $removed=0L
    foreach($object in $objects) {
        $path=[IO.Path]::GetFullPath($object.FullName)
        if(!$path.StartsWith($output+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Object cleanup boundary mismatch.'}
        Assert-PlainBuildPath $path
        $hash=(Get-FileHash -LiteralPath $path).Hash
        if((Get-FileHash -LiteralPath $path).Hash -cne $hash){throw 'Compiler object changed during cleanup.'}
        [IO.File]::Delete($path);$removed+=$object.Length
    }
    return $removed
}
function Snapshot-Boot([string]$Boot) {
    $pending=New-Object 'Collections.Generic.Stack[string]';$pending.Push($Boot)
    $rows=@()
    while($pending.Count) {
        foreach($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse Boot source refused.'}
            if($item.PSIsContainer) {if($item.Name -notin @('.git','tmp')){$pending.Push($item.FullName)}}
            elseif($item.Extension -in @('.c','.h','.asm','.inc','.mtpj','.ps1','.hex','.mot','.bin')) {
                $rows+=$item.FullName.Substring($Boot.Length+1)+' '+(Get-FileHash -LiteralPath $item.FullName).Hash
            }
        }
    }
    return Get-TestBinHash ([Text.Encoding]::UTF8.GetBytes((($rows|Sort-Object)-join "`n")))
}
try {
    foreach($app in $apps) {
        Assert-PlainBuildPath $app.Project
        $bin=Join-Path $app.Project ('DefaultBuild\'+$app.Name+'_update.bin')
        $before=(Get-FileHash -LiteralPath $bin).Hash
        $bootBefore=if(Test-Path -LiteralPath $app.Boot){Assert-PlainBuildPath $app.Boot;Snapshot-Boot $app.Boot}else{$null}
        $mtpj=Join-Path $app.Project ($app.Name+'.mtpj')
        $log=Join-Path $run ($app.Name+'.log');$err=$log+'.stderr'
        $start=[DateTime]::UtcNow
        $started=$false;$removed=0L
        try {
        $started=$true
        $process=Start-Process -FilePath $compiler -ArgumentList ('/br "'+$mtpj+'"') -WindowStyle Hidden `
            -RedirectStandardOutput $log -RedirectStandardError $err -PassThru -Wait
        $text=[IO.File]::ReadAllText($log)
        if($process.ExitCode -ne 0 -or $text -notmatch 'Build ended\(Error:0' -or
           $text -notmatch 'FF-filled timing BINs; original BIN unchanged\.') {
            throw ('Target App build/post-package hook failed: '+$app.Name+"`n"+($text -split "`n"|Select-Object -Last 45|Out-String))
        }
        # A rebuild may renew an older delivery from the current source tree.
        # The generator independently hashes its packaged input before/after.
        $after=(Get-FileHash -LiteralPath $bin).Hash
        if($null -ne $bootBefore -and (Snapshot-Boot $app.Boot) -cne $bootBefore){throw ('App-only build rewrote paired Boot sources/deliveries: '+$app.Name)}
        $auto=Join-Path $app.Project 'test_bins\auto'
        $rows=@(Import-Csv -LiteralPath (Join-Path $auto 'manifest.csv'))
        $source=[IO.File]::ReadAllBytes($bin);$capacity=[BitConverter]::ToUInt32($source,0x120)
        if(($rows|Measure-Object -Property input_bytes -Maximum).Maximum -ne $capacity){throw 'Automatic maximum-capacity BIN missing.'}
        $retained=0L
        foreach($row in $rows) {
            $path=Join-Path $auto $row.file
            if((Get-FileHash -LiteralPath $path).Hash -cne $row.sha256){throw 'Automatic BIN/manifest SHA mismatch.'}
            $retained+=(Get-Item -LiteralPath $path).Length
        }
        } finally {if($started){$removed=Clean-AppObjects $app $start $mtpj}}
        $warnings=@($text -split "`n"|Where-Object {$_ -match '\bW[0-9]{7}:'}|Sort-Object -Unique)
        $results+=[pscustomobject]@{project=$app.Name;result='PASS';source_sha256=$after;previous_sha256=$before;recompiled_bin_changed=($after -cne $before);boot_snapshot=$bootBefore;
            size_count=$rows.Count;maximum_bytes=$capacity;retained_test_bin_bytes=$retained;removed_object_bytes=$removed;warnings=$warnings}
        [IO.File]::WriteAllText((Join-Path $run 'results.json'),($results|ConvertTo-Json -Depth 4),(New-Object Text.UTF8Encoding($false)))
        [IO.File]::Delete($log);if(Test-Path -LiteralPath $err){[IO.File]::Delete($err)}
        Write-Host ('PASS App build/post-hook/generator input+Boot preservation: '+$app.Name+' / '+($capacity/1024)+' KiB / removed objects '+$removed+' B / recompiled BIN changed='+($after -cne $before))
    }
    $success=$true
} finally {
    $env:TEMP=$oldTemp;$env:TMP=$oldTmp
    # Development-tool temporary files are owned by this fresh invocation.
    $temp=Join-Path $run 'temp'
    $boundary=[IO.Path]::GetFullPath((Join-Path $repo 'tmp\internal\verification')).TrimEnd('\')
    if(!$run.StartsWith($boundary+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Verification cleanup escaped selected Tool.'}
    Assert-PlainBuildPath $temp
    $pending=New-Object 'Collections.Generic.Stack[string]';$pending.Push($temp);$items=@()
    while($pending.Count) {
        foreach($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if(!$item.FullName.StartsWith($temp+'\',[StringComparison]::OrdinalIgnoreCase) -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Temporary cleanup boundary mismatch.'}
            $items+=$item;if($item.PSIsContainer){$pending.Push($item.FullName)}
        }
    }
    foreach($item in $items|Where-Object {!$_.PSIsContainer}){[IO.File]::Delete($item.FullName)}
    foreach($item in $items|Where-Object {$_.PSIsContainer}|Sort-Object {$_.FullName.Length} -Descending){[IO.Directory]::Delete($item.FullName,$false)}
    [IO.Directory]::Delete((Join-Path $run 'temp'),$false)
    if(!$success){Write-Host ('Build failure evidence retained: '+$run)}
}
Write-Host ('PASS '+$results.Count+' target App builds. Evidence: '+$run)
