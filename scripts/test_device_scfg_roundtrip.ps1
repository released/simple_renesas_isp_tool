param([Parameter(Mandatory=$true)][string]$DeviceRoot,
    [ValidateSet('RL78Serial','RH850Can','V4')][string[]]$Group=@('RL78Serial','RH850Can','V4'),
    [string[]]$ProjectName=@())
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path.TrimEnd('\')
$out=Join-Path $tool ('tmp\internal\tests\device_scfg_'+[guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($out)|Out-Null
$cs=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\CS+\CC\CubeSuite+.exe'
if(!(Test-Path -LiteralPath $cs)){throw 'Installed CS+ is required; no installation is performed.'}
$names=@()
if('RL78Serial' -in $Group){foreach($mcu in @('F13','F24','G16','G22')){foreach($bus in @('UART','I2C','Xmodem')){
    $boot='RL78_'+$mcu+'_ISP_Boot_loader_'+$bus
    $names+=@($boot,($boot+'_app'),('RL78_'+$mcu+'_'+$(if($bus -eq 'I2C'){'I2C_Slave_Transfer'}else{$bus+'_Transfer'})))
}}}
if('RH850Can' -in $Group){foreach($suffix in @('CAN','CAN_FD')){
    $names+=@(('Sample_Project_RH850_S1_Boot_loader_'+$suffix),('Sample_Project_RH850_S1_Boot_loader_'+$suffix+'_app'),('Sample_Project_RH850_S1_'+$suffix+'_Transfer'))
}}
if('V4' -in $Group){$names+='Sample_Project_RH850_S1_CAN_FD_MultiCHv4'}
if($ProjectName.Count){
    foreach($name in $ProjectName){if($name -notin $names){throw ('Project is outside selected group: '+$name)}}
    $names=@($names|Where-Object {$_ -in $ProjectName})
}
function SourceManifest([string]$Folder){
    return (@(Get-ChildItem -LiteralPath $Folder -Recurse -File -Force|Where-Object {
        $_.FullName.Substring($Folder.Length+1) -notmatch '^(tmp|DefaultBuild|build|test_bins|\.git)(\\|$)'
    }|Sort-Object FullName|ForEach-Object { $_.FullName.Substring($Folder.Length+1)+' '+(Get-FileHash -LiteralPath $_.FullName).Hash }) -join "`n")
}
function Build-Copy([string]$Folder,[string]$Label){
    $name=Split-Path -Leaf $Folder;$log=Join-Path $out ($name+'_'+$Label+'_build.log')
    # Independent SC workers have private workspaces. Serialize CS+ build
    # processes so their installed IDE state cannot race across those workers.
    $buildMutex=New-Object Threading.Mutex($false,'Local\Codex_Device_SCFG_Target_Build')
    $held=$false
    try{
        try{$held=$buildMutex.WaitOne(1200000)}catch [Threading.AbandonedMutexException]{$held=$true}
        if(!$held){throw 'Timed out waiting for the private SC target-build slot.'}
        $process=Start-Process -FilePath $cs -ArgumentList ('/br DefaultBuild /lang en-US "'+(Join-Path $Folder ($name+'.mtpj'))+'"') -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr')
    }finally{if($held){$buildMutex.ReleaseMutex()};$buildMutex.Dispose()}
    $text=[IO.File]::ReadAllText($log)
    if(($null -ne $process.ExitCode -and $process.ExitCode -ne 0) -or $text -notmatch 'Build ended\(Error:0,' -or $text -match 'Failed:[1-9]'){throw ('SC roundtrip build failed: '+$log)}
    if($name -eq 'Sample_Project_RH850_S1_CAN_Transfer'){
        & (Join-Path $PSScriptRoot 'verify_rh850_transfer.ps1') -DeviceProject $Folder
    }
    Write-Host ('PASS SC copy rebuild/post-link: '+$name)
}
$originals=@{};$selected=@();$notApplicable=@()
foreach($name in $names){
    $source=Join-Path $DeviceRoot $name
    $configs=@(Get-ChildItem -LiteralPath $source -Filter '*.scfg' -File)
    if(!$configs.Count){$notApplicable+=$name;Write-Host ('N/A no SC-owned configuration: '+$name);continue}
    if($configs.Count -ne 1 -or $configs[0].BaseName -cne $name){throw 'Ambiguous SC configuration.'}
    $items=@(Get-Item -LiteralPath $source)+@(Get-ChildItem -LiteralPath $source -Recurse -Force)
    if($items|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Reparse project refused before copy.'}
    $originals[$source]=SourceManifest $source
    $dest=Join-Path $out $name;[IO.Directory]::CreateDirectory($dest)|Out-Null
    foreach($file in $items|Where-Object {!$_.PSIsContainer}){
        $relative=$file.FullName.Substring($source.Length+1)
        if($relative -match '^(tmp|DefaultBuild|build|test_bins|\.git)(\\|$)'){continue}
        $target=Join-Path $dest $relative
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target
    }
    $selected+=$dest
}
foreach($dest in $selected){
    $name=Split-Path -Leaf $dest
    $family=if($name.StartsWith('RL78_')){'RL78'}else{'RH850'}
    $sc=Join-Path ${env:ProgramFiles(x86)} ('Renesas Electronics\SmartConfigurator\'+$family+'\eclipse\SmartConfiguratorc.exe')
    if(!(Test-Path -LiteralPath $sc)){throw ('Installed '+$family+' SC is required.')}
    $proof=Join-Path $dest ('src\smc_gen\general\'+$(if($family -eq 'RL78'){'r_cg_macrodriver.h'}else{'r_smc_entry.h'}))
    if(!(Test-Path -LiteralPath $proof -PathType Leaf)){throw 'Missing private generated header for regeneration proof.'}
    # The vendor may reproduce byte-identical output. A test-only marker outside
    # user sections proves that generation actually rewrites the copied header.
    $marker='/* SC_ROUNDTRIP_PRIVATE_REGENERATION_PROOF */'
    [IO.File]::WriteAllText($proof,([IO.File]::ReadAllText($proof)+"`n"+$marker+"`n"),(New-Object Text.UTF8Encoding($false)))
    $before=SourceManifest (Join-Path $dest 'src\smc_gen')
    $log=Join-Path $out ($name+'_sc.log')
    $args='-nosplash -application com.renesas.smc.headlessGeneration -scfg "'+(Join-Path $dest ($name+'.scfg'))+'" -data "'+(Join-Path $out ('sc_workspace_'+$family))+'"'
    $process=Start-Process -FilePath $sc -ArgumentList $args -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr')
    if($null -eq $process.ExitCode -or $process.ExitCode -ne 0){throw ('Actual SC generation failed: '+$log)}
    if(!(Test-Path -LiteralPath $proof) -or [IO.File]::ReadAllText($proof).Contains($marker) -or (SourceManifest (Join-Path $dest 'src\smc_gen')) -ceq $before){throw ('No actual SC regeneration proof: '+$name)}
    Write-Host ('PASS actual vendor '+$family+' generation: '+$name)
}
foreach($dest in $selected){Build-Copy $dest 'regenerated'}
foreach($dest in $selected|Where-Object {(Split-Path -Leaf $_).EndsWith('_app')}){
    $boot=$dest.Substring(0,$dest.Length-4)
    $files=@(Get-ChildItem -LiteralPath (Join-Path $boot 'DefaultBuild') -File|Where-Object {$_.Extension -in @('.bin','.hex','.mot')})
    if(!$files.Count){throw 'No paired Boot delivery to preserve.'}
    $before=(@($files|ForEach-Object {$_.Name+' '+(Get-FileHash -LiteralPath $_.FullName).Hash}) -join "`n")
    Build-Copy $dest 'app_only'
    $after=(@($files|ForEach-Object {$_.Name+' '+(Get-FileHash -LiteralPath $_.FullName).Hash}) -join "`n")
    if($before -cne $after){throw 'App-only SC build rewrote Boot delivery.'}
    Write-Host ('PASS App-only Boot hashes: '+(Split-Path -Leaf $dest))
}
foreach($source in $originals.Keys){if((SourceManifest $source) -cne $originals[$source]){throw ('Original source changed during isolated audit: '+$source)}}
Write-Host ('PASS actual SC roundtrip count='+$selected.Count+'; N/A no SC='+$notApplicable.Count+'; original sources preserved; '+$out)
