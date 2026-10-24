param([Parameter(Mandatory=$true)][string]$DeviceRoot,[string]$WebRoot='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$count=0
foreach($family in @('F13','F24','G16','G22')){foreach($bus in @('UART','I2C','Xmodem')){
    $boot='RL78_'+$family+'_ISP_Boot_loader_'+$bus
    $suffix=if($bus -eq 'I2C'){'I2C_Slave_Transfer'}else{$bus+'_Transfer'}
    $transport=if($bus -eq 'I2C'){2}else{4}
    foreach($name in @($boot,($boot+'_app'),('RL78_'+$family+'_'+$suffix))){
        $folder=Join-Path $DeviceRoot $name
        $readme=[IO.File]::ReadAllText((Join-Path $folder 'README.md'))
        $rules=[IO.File]::ReadAllText((Join-Path $folder 'AGENTS.md'))
        if(!$readme.StartsWith('# '+$name) -or $readme -notmatch '512' -or
           $readme -notmatch 'app_irq_handlers.h' -and $name.EndsWith('_app')){throw ('Incomplete README: '+$name)}
        if($readme -match '[^\x00-\x7F]' -or $rules -match '[^\x00-\x7F]'){throw ('Expected English project entry: '+$name)}
        if($bus -eq 'I2C' -and ($readme -notmatch 'mailbox' -or $rules -notmatch 'I2C ISP')){throw 'I2C entry incorrectly describes UART.'}
        if($bus -eq 'Xmodem' -and ($readme -notmatch 'command 24' -or $readme -notmatch 'XMODEM-CRC128')){throw 'Missing Xmodem diagnostic description.'}
        foreach($link in [regex]::Matches($readme,'\]\(([^)]+)\)')){
            $target=$link.Groups[1].Value
            if($target -notmatch '^https://' -and !(Test-Path -LiteralPath (Join-Path $folder $target))){throw ('Broken project link: '+$name+' / '+$target)}
        }
        $count++
    }
    $protocol=[IO.File]::ReadAllText((Join-Path $DeviceRoot ($boot+'\PROTOCOL.md')))
    $id=if($bus -eq 'UART'){'0x55'}elseif($bus -eq 'I2C'){'0x49'}else{'0x58'}
    if(!$protocol.Contains($id) -or !$protocol.Contains('CRC32')){throw ('Missing protocol contract '+$boot)}
    $appHeader=[IO.File]::ReadAllText((Join-Path $DeviceRoot ($boot+'_app\app_image_config.h')))
    $value=[regex]::Match($appHeader,'(?m)^#define APP_IMAGE_UPDATE_TRANSPORTS\s+\(0x0([24])U\)').Groups[1].Value
    if([int]$value -ne $transport){throw ('Wrong documented transport '+$boot)}
    if($bus -eq 'I2C' -and $appHeader -match 'This UART project|0x02U\) /\* UART'){throw 'Stale I2C authoring comment.'}
}}
& (Join-Path $PSScriptRoot 'sync_isp_uart_guide.ps1') -WebRoot $WebRoot -CheckOnly
if($count -ne 36){throw 'Expected all36 project entries.'}
Write-Host 'PASS36 English serial README/rules,12protocol references,authoring comments and single-owner web guides.'
