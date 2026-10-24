param([Parameter(Mandatory=$true)][string]$DeviceRoot, [switch]$RebuildApps,
      [ValidateSet('F13','F24','G16','G22')][string[]]$Mcu=@('F13','F24','G16','G22'),
      [ValidateSet('UART','I2C','Xmodem')][string[]]$Bus=@('UART'))
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$common=Join-Path $tool 'firmware\common\isp_device'
function Snapshot-Boot([string]$Folder){
    return (@(Get-ChildItem -LiteralPath $Folder -Recurse -File | Where-Object {
        $_.FullName -notmatch '[\\/]tmp[\\/]' -and $_.Extension -ne '.rcpe'
    } | Sort-Object FullName | ForEach-Object {
        $_.FullName.Substring($Folder.Length)+' '+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }) -join "`n")
}
$selectedBuses=@($Bus);$selectedMcus=@($Mcu)
foreach($deviceBus in $selectedBuses){foreach($deviceMcu in $selectedMcus){
    $boot=Join-Path $DeviceRoot ('RL78_'+$deviceMcu+'_ISP_Boot_loader_'+$deviceBus)
    $app=Join-Path $DeviceRoot ('RL78_'+$deviceMcu+'_ISP_Boot_loader_'+$deviceBus+'_app')
    $suffix=if($deviceBus -eq 'I2C'){'I2C_Slave_Transfer'}else{$deviceBus+'_Transfer'}
    $transfer=Join-Path $DeviceRoot ('RL78_'+$deviceMcu+'_'+$suffix)
    $files=@('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h')
    if($deviceBus -eq 'UART' -and $deviceMcu -in @('F13','F24')){$files+=@('isp_rl78_board.c','isp_rl78_board.h','isp_rl78_runtime.c','isp_rl78_entry.c')}
    if($deviceBus -eq 'I2C'){$files+=@('isp_i2c_mailbox.c','isp_i2c_mailbox.h','isp_i2c.c','isp_i2c.h','isp_runtime_transport.inc')}
    if($deviceBus -eq 'Xmodem'){$files+=@('isp_xmodem.c','isp_xmodem.h','isp_runtime_transport.inc')}
    foreach($name in $files){
        $copy=Join-Path (Join-Path $boot 'isp') $name
        if((Get-FileHash -LiteralPath (Join-Path $common $name)).Hash -ne (Get-FileHash -LiteralPath $copy).Hash){throw ('Mirror drift: '+$copy)}
    }
    foreach($folder in @($boot,$app,$transfer)){
        $project=Join-Path $folder ((Split-Path -Leaf $folder)+'.mtpj')
        [xml]$xml=[IO.File]::ReadAllText($project)
        foreach($node in $xml.SelectNodes('//Instance[Type="File"]')){
            $relative=[string]$node.RelativePath
            if(!$relative){continue}
            if([IO.Path]::IsPathRooted($relative)){throw ('Absolute project dependency: '+$relative)}
            if(!(Test-Path -LiteralPath (Join-Path $folder $relative) -PathType Leaf)){throw ('Missing project dependency: '+$relative)}
        }
        $owned=@(Get-ChildItem -LiteralPath $folder -File | Where-Object {
            $_.Extension -in @('.md','.inc') -or $_.Name -in @('app_identity.h','app_image_config.h','app_irq_handlers.h')
        })
        if(Test-Path -LiteralPath (Join-Path $folder 'generated_projection')){
            $owned+=@(Get-ChildItem -LiteralPath (Join-Path $folder 'generated_projection') -File)
        }
        foreach($file in $owned){
            if([IO.File]::ReadAllText($file.FullName) -match '(?i)[A-Z]:[\\/](SourceCode|Users)[\\/]'){
                throw ('Personal path in reader/generated file: '+$file.FullName)
            }
        }
        Write-Host ('PASS project references/generated path check: '+(Split-Path -Leaf $folder))
    }
    if($deviceMcu -ne 'F24'){
        & (Join-Path $boot 'generate_irq_contract.ps1') -Mode Boot -CheckOnly
        & (Join-Path $boot 'generate_irq_contract.ps1') -Mode Boot -OutputProject $transfer -CheckOnly
        & (Join-Path $boot 'generate_irq_contract.ps1') -Mode App -AppProject $app -CheckOnly
    }else{
        foreach($kind in 'boot','app','transfer'){
            $project=switch($kind){boot {$boot} app {$app} transfer {$transfer}}
            & (Join-Path $boot 'tools\prepare_projects.ps1') -Kind $kind -Project $project -CheckOnly
        }
    }
    if($RebuildApps){
        $before=Snapshot-Boot $boot
        & (Join-Path $app 'build.ps1')
        $after=Snapshot-Boot $boot
        if($before -cne $after){throw ($deviceMcu+' '+$deviceBus+' App-only build changed Boot source/artifacts.')}
        Write-Host ('PASS '+$deviceMcu+' '+$deviceBus+' App-only wrapper; Boot source/artifact SHA256 manifest unchanged.')
    }
    Write-Host ('PASS '+$deviceMcu+' '+$deviceBus+' mirrored production C and generated inputs.')
}}
Write-Host 'Offline source/build checks only; no COM/programmer operation.'
