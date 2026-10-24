param([Parameter(Mandatory=$true)][string]$Project,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
$projectPath=(Resolve-Path -LiteralPath $Project).Path
$name=Split-Path -Leaf $projectPath
if($name -notmatch '^Sample_Project_RH850_S1_(?:ISP_Boot_loader_(UART|I2C|Xmodem)(?:_app)?|(UART|Xmodem)_Transfer|(I2C)_Slave_Transfer)$'){throw 'Only named RH850 serial projects are supported.'}
$selectedBus=@(@($Matches[1],$Matches[2],$Matches[3])|Where-Object {$_})[0]
$driver=if($selectedBus -eq 'I2C'){'Config_RIIC1'}else{'Config_UART1'}
$path=Join-Path $projectPath ($name+'.scfg')
[xml]$current=[IO.File]::ReadAllText($path)
if($CheckOnly){
    $names=@($current.SelectNodes('//tool[@id="SWComponent"]/configuration')|ForEach-Object {$_.name})
    if($driver -notin $names -or ($names|Where-Object {$_ -notin @('Config_TAUJ0_0','Config_OSTM0','Config_UART0',$driver)})){
        throw ('Serial SC component ownership mismatch: '+$name)
    }
    if($current.smc.general.platform.id -ne 'R7F701684'){throw 'Serial SC target is not R7F701684.'}
    return
}
# Explicit one-time initialization, not a normal build action. Preserve the
# EVM Clock selection while importing the selected vendor bus/pin configuration.
$reference=if($selectedBus -eq 'I2C'){'Sample_Project_RH850_S1_RIIC_I2C_Slave'}else{'Sample_Project_RH850_S1_UART1_TX_RX'}
$source=Join-Path (Split-Path -Parent $projectPath) ($reference+'\'+$reference+'.scfg')
[xml]$serial=[IO.File]::ReadAllText($source)
$clock=$serial.SelectSingleNode('//tool[@id="Clock"]')
[void]$serial.smc.ReplaceChild($serial.ImportNode($current.SelectSingleNode('//tool[@id="Clock"]'),$true),$clock)
foreach($node in @($serial.SelectNodes('//tool[@id="SWComponent"]/configuration'))){
    if($node.name -notin @('Config_TAUJ0_0','Config_OSTM0','Config_UART0',$driver)){[void]$node.ParentNode.RemoveChild($node)}
}
$settings=New-Object Xml.XmlWriterSettings
$settings.Encoding=New-Object Text.UTF8Encoding($false);$settings.Indent=$true
$writer=[Xml.XmlWriter]::Create($path,$settings)
try{$serial.Save($writer)}finally{$writer.Dispose()}
& $PSCommandPath -Project $projectPath -CheckOnly
Write-Host ('Initialized selected serial SC components: '+$name)
