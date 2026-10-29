# Existing device samples; Host firmware and standalone controller demos are excluded.
function Get-DeviceProjectCatalog([string]$DeviceRoot,[string[]]$ProjectName=@()) {
    $root=(Resolve-Path -LiteralPath $DeviceRoot).Path
    $triplets=@()
    foreach($mcu in @('F13','F24')) {
        foreach($bus in @('CAN','CAN_FD')) {
            if($mcu -eq 'F13' -and $bus -eq 'CAN_FD'){continue}
            $triplets+=,@(('RL78_'+$mcu+'_Boot_loader_'+$bus),('RL78_'+$mcu+'_'+$bus+'_Transfer'))
        }
    }
    foreach($mcu in @('F13','F24','G16','G22','RH850')) {
        $prefix=if($mcu -eq 'RH850'){'Sample_Project_RH850_S1'}else{'RL78_'+$mcu}
        foreach($bus in @('UART','I2C','Xmodem')) {
            $transfer=if($bus -eq 'I2C'){'I2C_Slave'}else{$bus}
            $triplets+=,@(($prefix+'_ISP_Boot_loader_'+$bus),($prefix+'_'+$transfer+'_Transfer'))
        }
    }
    foreach($bus in @('CAN','CAN_FD')) {
        $triplets+=,@(('Sample_Project_RH850_S1_Boot_loader_'+$bus),('Sample_Project_RH850_S1_'+$bus+'_Transfer'))
    }
    $selected=@()
    foreach($pair in $triplets) {
        foreach($role in @('boot','app','transfer')) {
            $name=switch($role){boot {$pair[0]} app {$pair[0]+'_app'} transfer {$pair[1]}}
            if($ProjectName.Count -and $name -notin $ProjectName){continue}
            $selected+=$name
            $project=Join-Path $root $name
            if(!(Test-Path -LiteralPath (Join-Path $project ($name+'.mtpj')) -PathType Leaf)) {
                throw ('Missing selected device project: '+$name)
            }
            [pscustomobject]@{Name=$name;Project=$project;Role=$role}
        }
    }
    if($ProjectName.Count -and @($ProjectName|Where-Object {$_ -notin $selected}).Count) {
        throw 'Unknown device project selection.'
    }
}
