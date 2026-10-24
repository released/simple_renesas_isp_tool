param([Parameter(Mandatory=$true)][string]$DeviceRoot,[switch]$RebuildApps)
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path
function Manifest([string]$Folder){
    return (@(Get-ChildItem -LiteralPath $Folder -Recurse -File -Force|Where-Object {
        $_.FullName.Substring($Folder.Length+1) -notmatch '^(tmp|\.git)(\\|$)'
    }|Sort-Object FullName|ForEach-Object {
        $_.FullName.Substring($Folder.Length+1)+' '+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }) -join "`n")
}
foreach($selectedBus in @('UART','I2C','Xmodem')){
    $name='Sample_Project_RH850_S1_ISP_Boot_loader_'+$selectedBus
    $boot=Join-Path $DeviceRoot $name
    $common=Join-Path $boot 'common'
    $files=@('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h')+
        $(if($selectedBus -eq 'I2C'){@('isp_i2c_mailbox.c','isp_i2c_mailbox.h')}elseif($selectedBus -eq 'Xmodem'){@('isp_xmodem.c','isp_xmodem.h')}else{@()})
    foreach($file in $files){
        if((Get-FileHash -LiteralPath (Join-Path $common $file)).Hash -cne
           (Get-FileHash -LiteralPath (Join-Path $tool ('firmware\common\isp_device\'+$file))).Hash){throw ('Portable mirror drift: '+$file)}
    }
    foreach($file in @('isp_rh850.h','isp_rh850_runtime.c','image_metadata_profile.c',$(if($selectedBus -eq 'I2C'){'isp_rh850_i2c.c'}else{'isp_rh850_uart.c'}))){
        if((Get-FileHash -LiteralPath (Join-Path $common $file)).Hash -cne
           (Get-FileHash -LiteralPath (Join-Path $tool ('firmware\common\isp_rh850\'+$file))).Hash){throw ('RH850 mirror drift: '+$file)}
    }
    $names=@($name,($name+'_app'),('Sample_Project_RH850_S1_'+$(if($selectedBus -eq 'I2C'){'I2C_Slave_Transfer'}else{$selectedBus+'_Transfer'})))
    foreach($projectName in $names){
        $project=Join-Path $DeviceRoot $projectName
        [xml]$xml=[IO.File]::ReadAllText((Join-Path $project ($projectName+'.mtpj')))
        $relative=@($xml.SelectNodes('//Instance[Type="File"]')|ForEach-Object {[string]$_.RelativePath})
        if($relative.Count -lt 15){throw 'Project source list unexpectedly empty.'}
        foreach($path in $relative){
            if([IO.Path]::IsPathRooted($path) -or !(Test-Path -LiteralPath (Join-Path $project $path) -PathType Leaf)){throw ('Missing/nonportable source: '+$path)}
            if($path -match 'hal[\\/]can|Config_CAN'){throw 'CAN source in serial project.'}
        }
        & (Join-Path $PSScriptRoot 'prepare_rh850_serial_scfg.ps1') -Project $project -CheckOnly
        foreach($source in @(Get-ChildItem -LiteralPath $project -Recurse -File|Where-Object {
            $_.FullName.Substring($project.Length+1) -notmatch '^(tmp|DefaultBuild)(\\|$)' -and $_.Extension -in @('.c','.h','.asm','.md','.ps1')
        })){
            $text=[IO.File]::ReadAllText($source.FullName)
            if($text -match '(?im)(?:[A-Z]:[\\/](?:SourceCode|Users|11101465)|/Users/)'){throw ('Local path in source/guide: '+$source.FullName)}
            if($source.Extension -eq '.ps1'){
                $tokens=$null;$errors=$null;[void][Management.Automation.Language.Parser]::ParseFile($source.FullName,[ref]$tokens,[ref]$errors)
                if($errors.Count){throw ('PowerShell parse failure: '+$source.FullName)}
            }
        }
    }
    if($RebuildApps){
        $before=Manifest $boot
        & (Join-Path $DeviceRoot ($name+'_app\build.ps1'))
        if((Manifest $boot) -cne $before){throw ($selectedBus+' App-only build changed paired Boot files.')}
        Write-Host ('PASS '+$selectedBus+' App-only rebuild;Boot source/artifact SHA256 manifest unchanged.')
    }
}
# Existing-target preflight must stop before modifying any of the nine folders.
$before=Manifest (Join-Path $DeviceRoot 'Sample_Project_RH850_S1_ISP_Boot_loader_UART')
$refused=$false
try{& (Join-Path $PSScriptRoot 'prepare_rh850_isp_projects.ps1') -DeviceRoot $DeviceRoot -Bus UART}catch{
    if($_.Exception.Message -notmatch 'Existing RH850 ISP project is never overwritten'){throw}
    $refused=$true
}
if(!$refused -or (Manifest (Join-Path $DeviceRoot 'Sample_Project_RH850_S1_ISP_Boot_loader_UART')) -cne $before){throw 'Creation preflight did not preserve existing projects.'}
Write-Host 'PASS9 RH850 serial projects:relative source ownership,mirrors,SC settings,PowerShell,public paths and creation refusal.'
