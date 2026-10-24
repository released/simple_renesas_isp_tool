param([Parameter(Mandatory=$true)][string]$DeviceRoot,
      [ValidateSet('UART','I2C','Xmodem')][string[]]$Bus=@('UART','I2C','Xmodem'))
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path.TrimEnd('\')
$sourceBoot=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_Boot_loader_CAN'
$sourceApp=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_Boot_loader_CAN_app'
$uartReference=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_UART1_TX_RX'
$i2cReference=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_RIIC_I2C_Slave'
$templates=Join-Path $tool 'firmware\common\isp_rh850'
$common=Join-Path $tool 'firmware\common\isp_device'
function Put([string]$Path,[string]$Text){
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))|Out-Null
    [IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false)))
}
function Macro([string]$Text,[string]$Name,[string]$Value){
    $pattern='(?m)^(#define\s+'+[regex]::Escape($Name)+'\s+)\([^\r\n)]+\)'
    if([regex]::Matches($Text,$pattern).Count -ne 1){throw ('Missing/duplicate macro '+$Name)}
    return [regex]::Replace($Text,$pattern,('${1}('+$Value+')'))
}
# Preflight the entire selection before creating anything; existing projects
# and reparse traversal are never overwritten or recursively moved/deleted.
foreach($required in @($sourceBoot,$sourceApp,$uartReference,$i2cReference,$templates,$common)){
    if(!(Test-Path -LiteralPath $required -PathType Container)){throw ('Missing source '+$required)}
    if((Get-Item -LiteralPath $required).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse source refused.'}
}
foreach($selectedBus in $Bus){foreach($suffix in @(('ISP_Boot_loader_'+$selectedBus),('ISP_Boot_loader_'+$selectedBus+'_app'),$(if($selectedBus -eq 'I2C'){'I2C_Slave_Transfer'}else{$selectedBus+'_Transfer'}))){
    $target=Join-Path $DeviceRoot ('Sample_Project_RH850_S1_'+$suffix)
    if(Test-Path -LiteralPath $target){throw ('Existing RH850 ISP project is never overwritten: '+$target)}
}}
foreach($selectedBus in $Bus){
    $bootName='Sample_Project_RH850_S1_ISP_Boot_loader_'+$selectedBus
    $transport=if($selectedBus -eq 'I2C'){2}else{4}
    $protocol=if($selectedBus -eq 'UART'){'0x55U'}elseif($selectedBus -eq 'I2C'){'0x49U'}else{'0x58U'}
    $contract=if($selectedBus -eq 'UART'){1}elseif($selectedBus -eq 'I2C'){2}else{3}
    foreach($kind in @('boot','app','transfer')){
        $name=switch($kind){boot{$bootName}app{$bootName+'_app'}transfer{'Sample_Project_RH850_S1_'+$(if($selectedBus -eq 'I2C'){'I2C_Slave_Transfer'}else{$selectedBus+'_Transfer'})}}
        $dest=Join-Path $DeviceRoot $name
        $from=if($kind -eq 'app'){$sourceApp}else{$sourceBoot}
        [IO.Directory]::CreateDirectory($dest)|Out-Null
        $files=@(Get-ChildItem -LiteralPath $from -Recurse -File -Force | Where-Object {
            $relative=$_.FullName.Substring($from.Length+1)
            $relative -notmatch '^(DefaultBuild|tmp|test_bins|\.git|generated_projection|hal)(\\|$)' -and
            ($relative -match '^(src|peripherals|FCL|FCL_user|tools)(\\|$)' -or
             $relative -match '^(boot.asm|cstart.asm|app_entry.asm|iodefine.h|custom_func.h|app_image_config.h|app_irq_handlers.h|app_runtime_log.h|\.gitignore)$' -or
             $_.Extension -in @('.mtpj','.scfg')) -and $_.Extension -notin @('.pdf','.jpg','.png','.log','.rcpe','.mtud')
        })
        foreach($file in $files){
            if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse source file refused.'}
            $relative=$file.FullName.Substring($from.Length+1)
            $relative=$relative.Replace((Split-Path -Leaf $from),$name)
            $path=Join-Path $dest $relative
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null
            if($file.Extension -in @('.c','.h','.asm','.mtpj','.ps1','.scfg') -or $file.Name -eq '.gitignore'){
                $text=[IO.File]::ReadAllText($file.FullName).Replace((Split-Path -Leaf $sourceApp),($bootName+'_app')).Replace((Split-Path -Leaf $sourceBoot),$bootName)
                if($file.Name -eq '.gitignore'){
                    $text=$text -replace '(?m)^!/DefaultBuild/flash_app_variable_CRC.hex\r?\n',''
                    if($kind -eq 'transfer'){$text=$text.Replace('boot0000_FFFF.hex',($name+'.hex'))}
                }
                if($file.Name -eq 'iodefine.h'){
                    $text=[regex]::Replace($text,'(?m)^/\* Options\s*:[^\r\n]*\r?\n(?:/\*\s{12,}[^\r\n]*\r?\n)+','/* Options    : DR7F701684.DVF -> iodefine.h; -def=off -f */'+"`n")
                }
                if($file.Extension -in @('.mtpj','.scfg')){
                    $text=$text.Replace((Split-Path -Leaf $from),$name)
                    if($kind -eq 'transfer'){$text=$text.Replace($bootName,$name)}
                }
                Put $path $text
            }else{Copy-Item -LiteralPath $file.FullName -Destination $path}
        }
        $irq=if($selectedBus -eq 'I2C'){@{240='IspI2cTxIsr';241='IspI2cEventIsr';242='IspI2cRxIsr';243='IspI2cEndIsr';84='r_Config_OSTM0_interrupt'}}else{@{122='IspUartRxIsr';123='IspUartErrorIsr';84='r_Config_OSTM0_interrupt'}}
        if($kind -eq 'app'){
            $appHeader=[IO.File]::ReadAllText((Join-Path $dest 'app_image_config.h'))
            $appHeader=Macro $appHeader 'APP_IMAGE_UPDATE_TRANSPORTS' ($transport.ToString()+'U')
            $appHeader=$appHeader.Replace('CAN','ISP '+$selectedBus)
            Put (Join-Path $dest 'app_image_config.h') $appHeader
            $lines=@('#ifndef APP_IRQ_HANDLERS_H','#define APP_IRQ_HANDLERS_H',
                '/* EIINT channel indexes from the RH850/F1KM-S1 interrupt source list.',
                ' * Global EIRET handlers have no channel= pragma; table generation owns routing. */',
                '#define APP_IRQ_HANDLERS(X) \')
            $channels=@($irq.Keys|Sort-Object)
            for($at=0;$at -lt $channels.Count;$at++){$ch=$channels[$at];$lines+=('    X('+$ch+', '+$irq[$ch]+')'+$(if($at -lt $channels.Count-1){' \'}else{''}))}
            $lines+='#endif';Put (Join-Path $dest 'app_irq_handlers.h') ($lines -join "`n")
        }
        Put (Join-Path $dest 'AGENTS.md') @"
# RH850 serial ISP device

- Independent $selectedBus Boot/App/Transfer; EVM R7F701684,16MHz,CPU120.
- Boot64KiB/App10000,metadata10100,EBASE10200,INTBP10400/full512,code11000.
- Neutral metadata/profile,ISP configuration and AA/55 RAM handoff stay separate.
- User edits App C headers; CS+/CC-RH and PowerShell, no Python/JSON inputs.
- Global EIRET ISR functions have no channel=; main owns protocol/Flash/logs.
- Preserve FCL RAM copy/geometry,ECC-safe RAM reader,replay/CRC/version guards.
- App-only builds never write paired Boot. Strict C90 and English comments.
- Offline preparation only; no COM/programmer/hardware or Git publication.
"@
        Put (Join-Path $dest 'README.md') @"
# $name

Independent RH850/F1KM-S1 $selectedBus $kind project for EVM R7F701684/16MHz.
This is offline development; board electrical/IRQ/Flash/reset acceptance is not established.
App and Transfer require the matching $bootName directory as a sibling.

| Item | Configuration |
| --- | --- |
| Boot/App | Boot64KiB,App10000,metadata10100/256bytes,EBASE10200,INTBP10400/512entries,code11000 |
| Data interface | $(if($selectedBus -eq 'I2C'){'RIIC1 SCL P8_1,SDA P8_0,7-bit0x20,100kHz'}else{'RLIN31 TX P0_5,RX P0_4,115200/8-N-1'}) |
| Diagnostic Log | RLIN30 TX P10_10,RX P10_9,115200;independent text link |
| Flash | FCL32KiB App erase,256-byte page write;RAM copy/ECC-safe read guards |

Edit app_image_config.h and app_irq_handlers.h for App identity and IRQs.
image_metadata_config.h owns MCU/Flash/format;isp_transport_config.h owns the selected bus.
Use build.ps1 from this project root with installed CS+/CC-RH and PowerShell.
Boot uses DefaultBuild/boot0000_FFFF.hex;App uses its project-named _update.bin;
Transfer uses its project-named standalone HEX. The build report summarizes actual linked data.
After Smart Configurator Generate Code, run the same build.ps1 entry; its hooks
recreate owned projections and validate the linked metadata, all512 IRQ entries
and RAM execution sections. App-only builds preserve the paired Boot.
ENTER/END reset waits for a successfully transmitted complete reply. In the
Xmodem variant, a control-byte TX failure cancels streaming without reset.
Source warnings are cleared; the current CC-RH environment still reports
license diagnostics and the reviewed prebuilt-library .text alignment mismatch.
No GitHub publication or physical success is implied by local source/build/model checks.
"@
        $wrapper="param([ValidateSet('boot','app','transfer','all')][string]`$Kind='$kind')`n`$ErrorActionPreference='Stop'`n"
        $wrapper+="`$boot=Join-Path (Split-Path -Parent `$PSScriptRoot) '$bootName'`n"
        $wrapper+="& (Join-Path `$boot 'tools\build.ps1') -Kind `$Kind -Project `$PSScriptRoot`n"
        Put (Join-Path $dest 'build.ps1') $wrapper
    }
    $boot=Join-Path $DeviceRoot $bootName
    [IO.Directory]::CreateDirectory((Join-Path $boot 'common'))|Out-Null
    foreach($file in @('image_metadata.c','image_metadata.h','image_metadata_config.h','boot_app_handoff.h','device_image.c','device_image.h','device_read.c','device_cpu.asm','device_flash.c','flash_geometry.c','flash_geometry.h')){
        Copy-Item -LiteralPath (Join-Path $sourceBoot ('common\'+$file)) -Destination (Join-Path $boot ('common\'+$file))
    }
    $imageConfig=[IO.File]::ReadAllText((Join-Path $boot 'common\image_metadata_config.h'))
    $imageConfig=Macro $imageConfig 'IMAGE_MANIFEST_MAGIC' '0x00505349UL'
    $imageConfig=Macro $imageConfig 'IMAGE_POLICY_WORD' $(if($selectedBus -eq 'I2C'){'0x02012003UL'}else{'0x04012003UL'})
    $imageConfig=$imageConfig.Replace('/* CAN + NUL */','/* ISP + NUL */').Replace('independent of CAN','independent of ISP')
    Put (Join-Path $boot 'common\image_metadata_config.h') $imageConfig
    $image=[IO.File]::ReadAllText((Join-Path $boot 'common\device_image.c'))
    $image=$image.Replace('#include "can_boot_config.h"','')
    $image=$image.Substring(0,$image.IndexOf('unsigned long Device_ImageInfo('))
    Put (Join-Path $boot 'common\device_image.c') $image
    Put (Join-Path $boot 'common\device_platform.h') "#include `"isp_rh850.h`"`n"
    foreach($file in @('isp_rh850.h','isp_rh850_runtime.c','image_metadata_profile.c',$(if($selectedBus -eq 'I2C'){'isp_rh850_i2c.c'}else{'isp_rh850_uart.c'}))){
        Copy-Item -LiteralPath (Join-Path $templates $file) -Destination (Join-Path $boot ('common\'+$file))
    }
    foreach($file in @('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h','isp_bus_contract.h')+$(if($selectedBus -eq 'I2C'){@('isp_i2c_mailbox.c','isp_i2c_mailbox.h')}elseif($selectedBus -eq 'Xmodem'){@('isp_xmodem.c','isp_xmodem.h')}else{@()})){
        Copy-Item -LiteralPath (Join-Path $common $file) -Destination (Join-Path $boot ('common\'+$file))
    }
    $transportConfig=@"
#ifndef ISP_TRANSPORT_CONFIG_H
#define ISP_TRANSPORT_CONFIG_H
/* EVM data pins are separate from RLIN30 diagnostic text.
 * IRQ indexes are channels, not RL78 vector byte addresses.
 */
#define ISP_CONTRACT_ID ($contract`U)
#define ISP_UPDATE_TRANSPORT ($transport`U)
#define ISP_BOOT_PROTOCOL_ID ($protocol)
#define ISP_BOOT_PROTOCOL_VERSION (1U)
#define ISP_BUS_RATE ($(if($selectedBus -eq 'I2C'){'100000UL'}else{'115200UL'}))
#define ISP_POLICY_UART_BAUD ($(if($selectedBus -eq 'I2C'){'0UL'}else{'115200UL'}))
#define ISP_LINK_PARAMETER_0 ($(if($selectedBus -eq 'I2C'){'100000UL'}else{'0UL'}))
#define ISP_LINK_PARAMETER_1 ($(if($selectedBus -eq 'I2C'){'0x20UL'}else{'0UL'}))
#define ISP_I2C_ADDRESS (0x20U)
#define ISP_INTERBYTE_TIMEOUT_MS (200U)
#define ISP_BOOT_IDLE_TIMEOUT_MS (30000U)
#endif
"@
    Put (Join-Path $boot 'common\isp_transport_config.h') $transportConfig
    # Generated driver initializers retain the vendor source/disclaimer.
    # Owned ISR adapters replace their user handlers; no ISR protocol work.
    foreach($kind in @('boot','app','transfer')){
        $name=switch($kind){boot{$bootName}app{$bootName+'_app'}transfer{'Sample_Project_RH850_S1_'+$(if($selectedBus -eq 'I2C'){'I2C_Slave_Transfer'}else{$selectedBus+'_Transfer'})}}
        $dest=Join-Path $DeviceRoot $name
        foreach($file in @('Config_UART0.c','Config_UART0.h')){
            Copy-Item -LiteralPath (Join-Path $uartReference ('src\smc_gen\Config_UART0\'+$file)) -Destination (Join-Path $dest ('src\smc_gen\Config_UART0\'+$file)) -Force
        }
        $driver=if($selectedBus -eq 'I2C'){'Config_RIIC1'}else{'Config_UART1'}
        $reference=if($selectedBus -eq 'I2C'){$i2cReference}else{$uartReference}
        [IO.Directory]::CreateDirectory((Join-Path $dest ('src\smc_gen\'+$driver)))|Out-Null
        foreach($file in @(($driver+'.c'),($driver+'.h'))){
            Copy-Item -LiteralPath (Join-Path $reference ('src\smc_gen\'+$driver+'\'+$file)) -Destination (Join-Path $dest ('src\smc_gen\'+$driver+'\'+$file))
        }
        if($selectedBus -eq 'I2C'){
            Copy-Item -LiteralPath (Join-Path $i2cReference 'src\smc_gen\general\r_cg_riic.h') -Destination (Join-Path $dest 'src\smc_gen\general\r_cg_riic.h')
        }
        & (Join-Path $PSScriptRoot 'prepare_rh850_serial_scfg.ps1') -Project $dest
    }
    # Prepare the independent local build helper from the inspected CAN
    # golden helper, changing only serial-specific inputs/source ownership.
    & (Join-Path $PSScriptRoot 'prepare_rh850_isp_tools.ps1') -BootProject $boot -Bus $selectedBus
    Write-Host ('Prepared independent RH850 '+$selectedBus+' triplet;build/board acceptance is separate.')
}
