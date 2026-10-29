param([Parameter(Mandatory=$true)][string]$DeviceRoot,
    [ValidateSet('UART','I2C','Xmodem')][string[]]$Bus=@('UART','I2C','Xmodem'),
    [string]$Configurator='')
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path.TrimEnd('\')
if(!$Configurator){$Configurator=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\SmartConfigurator\RH850\eclipse\SmartConfiguratorc.exe'}
if(!(Test-Path -LiteralPath $Configurator -PathType Leaf)){throw 'Installed RH850 Smart Configurator is required.'}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$testScope=New-OwnedTestWorkspace 'rs' -ShortPath
$out=$testScope.Path
try {
function SourceManifest([string]$Folder){
    return (@(Get-ChildItem -LiteralPath $Folder -File -Recurse -Force|Where-Object {
        $_.FullName.Substring($Folder.Length+1) -notmatch '^(tmp|DefaultBuild|test_bins|\.git)(\\|$)'
    }|Sort-Object FullName|ForEach-Object {
        $_.FullName.Substring($Folder.Length+1)+' '+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }) -join "`n")
}
$originals=@{}
foreach($selectedBus in $Bus){
    $bootName='Sample_Project_RH850_S1_ISP_Boot_loader_'+$selectedBus
    $names=@($bootName,($bootName+'_app'),('Sample_Project_RH850_S1_'+$(if($selectedBus -eq 'I2C'){'I2C_Slave_Transfer'}else{$selectedBus+'_Transfer'})))
    foreach($name in $names){
        $source=Join-Path $DeviceRoot $name
        $items=@(Get-Item -LiteralPath $source)+@(Get-ChildItem -LiteralPath $source -Recurse -Force)
        if($items|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Reparse project refused before copy.'}
        $originals[$source]=SourceManifest $source
        $dest=Join-Path $out $name
        if(Test-Path -LiteralPath $dest){throw 'Roundtrip copy already exists.'}
        [IO.Directory]::CreateDirectory($dest)|Out-Null
        foreach($file in $items|Where-Object {!$_.PSIsContainer}){
            $relative=$file.FullName.Substring($source.Length+1)
            if($relative -match '^(tmp|DefaultBuild|test_bins|\.git)(\\|$)'){continue}
            $target=Join-Path $dest $relative
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $target
        }
        $scfg=Join-Path $dest ($name+'.scfg')
        $before=SourceManifest (Join-Path $dest 'src\smc_gen')
        $log=Join-Path $out ($name+'_sc.log')
        $args='-nosplash -application com.renesas.smc.headlessGeneration -scfg "'+$scfg+'" -data "'+(Join-Path $out 'sc_workspace')+'"'
        $process=Start-Process -FilePath $Configurator -ArgumentList $args -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr')
        if($null -eq $process.ExitCode -or $process.ExitCode -ne 0){throw ('SC generation failed or exit status unavailable: '+$log)}
        Get-Content -LiteralPath $log | Select-Object -Last 12 | Write-Host
        $after=SourceManifest (Join-Path $dest 'src\smc_gen')
        if($before -ceq $after){throw ('No generated source change;cannot establish actual regeneration: '+$name)}
        & (Join-Path $PSScriptRoot 'prepare_rh850_serial_scfg.ps1') -Project $dest -CheckOnly
        Write-Host ('PASS actual SC regeneration: '+$name)
    }
    foreach($name in $names){
        & (Join-Path $out ($name+'\build.ps1'))
        if($LASTEXITCODE){throw ('Roundtrip build failed: '+$name)}
    }
    if($LASTEXITCODE){throw 'Roundtrip build failed.'}
    $boot=Join-Path $out $bootName
    $bootFiles=@(Get-ChildItem -LiteralPath (Join-Path $boot 'DefaultBuild') -File|Where-Object {$_.Extension -in @('.hex','.mot','.bin')})
    $before=(@($bootFiles|ForEach-Object {$_.Name+' '+(Get-FileHash -LiteralPath $_.FullName).Hash}) -join "`n")
    & (Join-Path $out ($bootName+'_app\build.ps1'))
    $after=(@($bootFiles|ForEach-Object {$_.Name+' '+(Get-FileHash -LiteralPath $_.FullName).Hash}) -join "`n")
    if($before -cne $after){throw 'App-only rebuild changed paired Boot delivery.'}
    Write-Host ('PASS '+$selectedBus+' SC -> projection -> linked metadata/512IRQ/FCL/RAM and App-only Boot hash.')
}
foreach($source in $originals.Keys){if((SourceManifest $source) -cne $originals[$source]){throw ('Original source changed: '+$source)}}
Write-Host ('PASS isolated roundtrip. Original sources preserved. Evidence: '+$out)

} finally { Remove-OwnedTestWorkspace $testScope }
