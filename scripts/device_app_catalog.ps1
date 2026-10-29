# Existing MCU device Apps only; host and standalone Transfer are not targets.
function Get-DeviceTimingApps([string]$DeviceRoot,[string[]]$ProjectName=@()) {
    $root=(Resolve-Path -LiteralPath $DeviceRoot).Path
    $names=@('RL78_F13_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_app','RL78_F24_Boot_loader_CAN_FD_app')
    foreach($mcu in @('F13','F24','G16','G22')) {
        foreach($bus in @('UART','I2C','Xmodem')) {$names+='RL78_'+$mcu+'_ISP_Boot_loader_'+$bus+'_app'}
    }
    foreach($bus in @('CAN','CAN_FD')) {$names+='Sample_Project_RH850_S1_Boot_loader_'+$bus+'_app'}
    foreach($bus in @('UART','I2C','Xmodem')) {$names+='Sample_Project_RH850_S1_ISP_Boot_loader_'+$bus+'_app'}
    if($ProjectName.Count) {
        if(@($ProjectName|Where-Object {$_ -notin $names}).Count){throw 'Unknown App selection.'}
        $names=@($names|Where-Object {$_ -in $ProjectName})
    }
    foreach($name in $names) {
        $project=Join-Path $root $name
        $boot=Join-Path $root $name.Substring(0,$name.Length-4)
        if(!(Test-Path -LiteralPath $project -PathType Container)) {
            throw ('Missing device App: '+$name)
        }
        [pscustomobject]@{Name=$name;Project=$project;Contract=$project;Boot=$boot}
    }
}
