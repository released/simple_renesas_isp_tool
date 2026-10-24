param([Parameter(Mandatory=$true)][string]$BootProject)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=(Resolve-Path -LiteralPath $BootProject).Path.TrimEnd('\')
$name=Split-Path -Leaf $source
if($name -notin @('Sample_Project_RH850_S1_Boot_loader_CAN','Sample_Project_RH850_S1_Boot_loader_CAN_FD')){throw 'Expected an existing RH850 CAN Boot.'}
$items=@(Get-Item -LiteralPath $source)+@(Get-ChildItem -LiteralPath $source -Recurse -Force)
if($items | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Reparse source refused.'}
$out=Join-Path $repo ('tmp\internal\tests\rh850_can_all6_'+[guid]::NewGuid().ToString('N').Substring(0,8))
$dest=Join-Path $out $name
[IO.Directory]::CreateDirectory($dest)|Out-Null
$before=@{}
foreach($file in $items | Where-Object {!$_.PSIsContainer}){
    $relative=$file.FullName.Substring($source.Length+1)
    if($relative -match '^(tmp|DefaultBuild|build|\.git)(\\|$)'){continue}
    $before[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash
    $target=Join-Path $dest $relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target))|Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
$path=Join-Path $dest ($name+'.mtpj');[xml]$xml=[IO.File]::ReadAllText($path)
$defs=$xml.SelectSingleNode('//COptionD-0')
for($channel=0;$channel -lt 6;$channel++){$defs.InnerText+="`nENABLE_PORT_CAN"+$channel+'=1'}
$xml.Save($path)
$cs=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\CS+\CC\CubeSuite+.exe'
$log=Join-Path $out 'all6_build.log'
$proc=Start-Process -FilePath $cs -ArgumentList ('/br DefaultBuild /lang en-US "'+$path+'"') -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr')
$text=[IO.File]::ReadAllText($log)
if(($null -ne $proc.ExitCode -and $proc.ExitCode -ne 0) -or $text -notmatch 'Build ended\(Error:0,' -or $text -match 'Failed:[1-9]'){
    Get-Content -LiteralPath $log -Tail 32 | Write-Host
    throw ('All-six target/RAM guard failed: '+$log)
}
Get-Content -LiteralPath $log -Tail 15 | Write-Host
foreach($file in $before.Keys){if((Get-FileHash -LiteralPath $file).Hash -cne $before[$file]){throw ('Original changed: '+$file)}}
Write-Host ('PASS actual all-six-channel Boot target/ROM/RAM guards; original source hashes preserved. '+$out)
