param([Parameter(Mandatory=$true)][string]$DeviceRoot,
      [ValidateSet('F13','F24','G16','G22')][string[]]$Mcu=@('F13','F24','G16','G22'),
      [ValidateSet('I2C','Xmodem')][string[]]$Bus=@('I2C','Xmodem'))
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path
$common=Join-Path $tool 'firmware\common\isp_device'
$utf8=New-Object Text.UTF8Encoding($false)
function Put([string]$Path,[string]$Text){[IO.File]::WriteAllText($Path,$Text,$utf8)}
function Replace-Macro([string]$Text,[string]$Name,[string]$Value){
    $pattern='(?m)^(#define\s+'+[regex]::Escape($Name)+'\s+)\([^\r\n]+?\)'
    if([regex]::Matches($Text,$pattern).Count -ne 1){throw ('Expected one macro '+$Name)}
    [regex]::Replace($Text,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$m.Groups[1].Value+'('+$Value+')'})
}
foreach($family in $Mcu){foreach($kind in $Bus){foreach($suffix in @(('ISP_Boot_loader_'+$kind),('ISP_Boot_loader_'+$kind+'_app'),$(if($kind -eq 'I2C'){'I2C_Slave_Transfer'}else{'Xmodem_Transfer'}))){
    $target=Join-Path $DeviceRoot ('RL78_'+$family+'_'+$suffix)
    if(Test-Path -LiteralPath $target){throw ('Existing project is never overwritten: '+$target)}
}}}
foreach($family in $Mcu){foreach($kind in $Bus){
    $oldBoot='RL78_'+$family+'_ISP_Boot_loader_UART';$newBoot='RL78_'+$family+'_ISP_Boot_loader_'+$kind
    $oldTransfer='RL78_'+$family+'_UART_Transfer'
    $newTransfer='RL78_'+$family+'_'+$(if($kind -eq 'I2C'){'I2C_Slave_Transfer'}else{'Xmodem_Transfer'})
    $boot=Join-Path $DeviceRoot $newBoot
    foreach($role in 'boot','app','transfer'){
        $oldName=switch($role){boot {$oldBoot} app {$oldBoot+'_app'} transfer {$oldTransfer}}
        $newName=switch($role){boot {$newBoot} app {$newBoot+'_app'} transfer {$newTransfer}}
        $from=Join-Path $DeviceRoot $oldName;$dest=Join-Path $DeviceRoot $newName
        [IO.Directory]::CreateDirectory($dest)|Out-Null
        $all=@(Get-ChildItem -LiteralPath $from -Recurse -File -Force | Where-Object {
            $_.FullName.Substring($from.Length+1) -notmatch '^(DefaultBuild|tmp|test_bins|\.git|generated_projection)(\\|$)' -and
            $_.Extension -notin @('.rcpe','.scfg','.log') -and $_.Name -notlike 'QualityReport*'
        })
        foreach($file in $all){
            if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse source is not allowed.'}
            $relative=$file.FullName.Substring($from.Length+1).Replace($oldBoot,$newBoot).Replace($oldTransfer,$newTransfer)
            $out=Join-Path $dest $relative;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($out))|Out-Null
            if($file.Extension -in @('.c','.h','.asm','.inc','.ps1','.md','.mtpj','.bat','.cmd','.json','.txt')){
                $text=[IO.File]::ReadAllText($file.FullName).Replace($oldBoot,$newBoot).Replace($oldTransfer,$newTransfer)
                if($file.Name -in @('isp_entry.c','isp_rl78_entry.c','app_main.c')){
                    $text=$text.Replace('ISP UART App:','ISP '+$kind+' App:').Replace('ISP UART Custom Transfer: binary UART1; diagnostic UART0','ISP '+$kind+' Custom Transfer; diagnostic UART0')
                }
                if($file.Name -eq 'build.ps1'){$text=$text.Replace('_ISP_Boot_loader_UART','_ISP_Boot_loader_'+$kind).Replace('_UART_Transfer','_'+$(if($kind -eq 'I2C'){'I2C_Slave_Transfer'}else{'Xmodem_Transfer'})).Replace(' UART ',(' '+$kind+' '))}
                if($kind -eq 'I2C'){
                    if($file.Name -eq 'package_images.ps1'){$text=$text.Replace("@(0x40,'_IspUartRxIsr')","@(0x28,'_IspI2cIsr')")}
                    if($file.Name -in @('app_config.ps1')){$text=$text.Replace('UPDATE_TRANSPORTS -ne 4','UPDATE_TRANSPORTS -ne 2').Replace('UART DEVICE_APP','I2C DEVICE_APP')}
                    if($file.Name -eq 'app_image_config.h'){$text=Replace-Macro $text 'APP_IMAGE_UPDATE_TRANSPORTS' '0x02U';$text=$text.Replace('0x04 = UART','0x02 = I2C').Replace('4U: UART','2U: I2C')}
                    if($file.Name -eq 'image_metadata_config.h' -and $role -eq 'boot'){$text=Replace-Macro $text 'IMAGE_POLICY_WORD' '0x02012003UL'}
                    if($file.Name -eq 'app_irq_handlers.h'){
                        $text=$text.Replace('IspUartRxIsr','IspI2cIsr')
                        $rx=if($family -eq 'G16'){'0x20'}elseif($family -eq 'G22'){'0x26'}else{'0x40'}
                        $irq=if($family -eq 'G16'){'0x2C'}elseif($family -eq 'G22'){'0x2A'}else{'0x28'}
                        $text=$text.Replace($rx,$irq)
                    }
                    if($file.Name -eq 'isp_rl78_board.h'){$text=$text.Replace('#include "isp_device.h"',"#include `"isp_i2c.h`"`n#include `"isp_device.h`"")}
                    if($file.Name -in @('generate_irq_contract.ps1','prepare_projects.ps1')){
                        $text=$text.Replace('IspUartRxIsr','IspI2cIsr')
                        $slot=if($family -eq 'G16'){'0x2040'}elseif($family -eq 'G22'){'0x204C'}else{'0x2080'}
                        $newSlot=if($family -eq 'G16'){'0x2058'}elseif($family -eq 'G22'){'0x2054'}else{'0x2050'}
                        $text=$text.Replace($slot,$newSlot).Replace('_IspI2cIsr .VECTOR 0x40','_IspI2cIsr .VECTOR 0x28')
                        if($family -eq 'F24'){$text=$text.Replace('CALL !!_IspUartStop',"CALL !!_IspUartStop`n    CALL !!_IspI2cStop").Replace('.EXTERN _IspUartStop',".EXTERN _IspUartStop``n.EXTERN _IspI2cStop")}
                    }
                }
                if($file.Name -eq 'generate_irq_contract.ps1'){$text=$text.Replace("ISP_UART_BAUD = (Macro 'ISP_UART_BAUD')","ISP_UART_BAUD = (Macro 'ISP_POLICY_UART_BAUD')")}
                if($file.Name -eq 'device_common.ps1'){$text=$text.Replace('Get-DeviceMacro ISP_UART_BAUD','Get-DeviceMacro ISP_POLICY_UART_BAUD')}
                if($file.Name -eq 'image_metadata_profile.c'){$text=$text.Replace('ISP_UART_BAUD,','ISP_POLICY_UART_BAUD,')}
                if($file.Name -eq 'generate_update_image.ps1'){
                    $text=$text.Replace(' -ne 115200)'," -ne (Get-EquValue 'ISP_UART_BAUD'))")
                    $text=$text.Replace('$manifestProtocolId -ne 0x55',"`$manifestProtocolId -ne ((Get-EquValue 'IMAGE_MANIFEST_PROTOCOL') -band 65535)")
                }
                if($file.Name -eq 'prepare_projects.ps1'){$text=$text.Replace('ISP_DEVICE_"+$ImageKind.ToUpperInvariant()+"`n"','ISP_DEVICE_"+$ImageKind.ToUpperInvariant()+"`nISP_'+$kind.ToUpperInvariant()+'`n"')}
                Put $out $text
            }else{Copy-Item -LiteralPath $file.FullName -Destination $out}
        }
    }
    foreach($file in @('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h','isp_runtime_transport.inc')+$(if($kind -eq 'I2C'){@('isp_i2c_mailbox.c','isp_i2c_mailbox.h','isp_i2c.c','isp_i2c.h')}else{@('isp_xmodem.c','isp_xmodem.h')})){
        Copy-Item -LiteralPath (Join-Path $common $file) -Destination (Join-Path $boot ('isp\'+$file))
    }
    $config=[IO.File]::ReadAllText((Join-Path $boot 'isp_transport_config.h'))
    $config=Replace-Macro $config 'ISP_BOOT_PROTOCOL_ID' $(if($kind -eq 'I2C'){'0x49U'}else{'0x58U'})
    if($kind -eq 'I2C'){
        $config=Replace-Macro $config 'ISP_LINK_PARAMETER_0' '100000UL'
        $config=Replace-Macro $config 'ISP_LINK_PARAMETER_1' '0x20UL'
    }
    $macros="`n/* One compiled update interface; UART0 remains diagnostic text. */`n#define ISP_$($kind.ToUpperInvariant()) 1`n"+
        '#define ISP_CONTRACT_ID ('+$(if($kind -eq 'I2C'){'2U'}else{'3U'})+")`n"+
        '#define ISP_UPDATE_TRANSPORT ('+$(if($kind -eq 'I2C'){'2U'}else{'4U'})+")`n"+
        '#define ISP_BUS_RATE ('+$(if($kind -eq 'I2C'){'100000UL'}else{'115200UL'})+")`n"+
        '#define ISP_POLICY_UART_BAUD ('+$(if($kind -eq 'I2C'){'0UL'}else{'115200UL'})+")`n#define ISP_I2C_ADDRESS (0x20U)`n"
    Put (Join-Path $boot 'isp_transport_config.h') ($config.Replace('#endif',$macros+'#endif'))
    $runtimeName=if($family -in @('G16','G22')){'isp_runtime.c'}else{'isp_rl78_runtime.c'}
    $runtimePath=Join-Path $boot ('isp\'+$runtimeName)
    $runtime=[IO.File]::ReadAllText($runtimePath)
    $runtime=$runtime.Replace('static IspStream g_stream;',"#include <string.h>`n#if defined(ISP_XMODEM)`n#include `"isp_xmodem.h`"`n#define g_input g_isp_input`n#define g_stream g_input.stream`n#else`nstatic IspStream g_stream;`n#endif")
    $runtime=$runtime.Replace('static IspDevice g_device;',"#if defined(ISP_DEVICE_BOOT) && defined(ISP_XMODEM)`n#define g_device g_isp_device`n#else`nstatic IspDevice g_device;`n#endif")
    $runtime=$runtime.Replace('static IspPacket g_request, g_reply;',"#if defined(ISP_DEVICE_BOOT) && defined(ISP_XMODEM)`n#define g_request g_isp_request`n#define g_reply g_isp_reply`n#else`nstatic IspPacket g_request, g_reply;`n#endif")
    $runtime=$runtime.Replace('1U, 0U, 4U, 64U','ISP_CONTRACT_ID, 0U, ISP_UPDATE_TRANSPORT, 64U').Replace('IMAGE_BOOT_BUILD, ISP_UART_BAUD,','IMAGE_BOOT_BUILD, ISP_BUS_RATE,')
    $runtime=$runtime.Substring(0,$runtime.IndexOf('void IspRuntimePoll(void)'))+'#include "isp_runtime_transport.inc"'+"`n"
    if($kind -eq 'I2C'){
        $runtime=$runtime.Replace('g_last_rx = IspTick(); g_last_activity = g_last_rx;','g_last_rx = IspTick(); g_last_activity = g_last_rx; IspI2cInit();')
        $runtime=$runtime.Replace('DI(); IspUartStop(); IspTimerStop();','DI(); IspI2cStop(); IspUartStop(); IspTimerStop();')
    }
    Put $runtimePath $runtime
    Copy-Item -LiteralPath (Join-Path $common 'app_build_report.ps1') -Destination (Join-Path $boot $(if($family -eq 'F24'){'tools\app_build_report.ps1'}else{'app_build_report.ps1'}))
    if($kind -eq 'I2C'){
        $boardName=if($family -in @('G16','G22')){'isp_board.c'}else{'isp_rl78_board.c'}
        $path=Join-Path $boot ('isp\'+$boardName);$text=[IO.File]::ReadAllText($path)
        # Disable the binary UART route for this independent I2C project.
        $text=$text.Replace('SRMK1 = 0U;','SRMK1 = 1U;')
        if($family -in @('G16','G22')){$text=$text.Replace('SS0 = 15U;','SS0 = 3U;')}
        else{$text=$text.Replace('SS1 |= 3U;','ST1 |= 3U;')}
        Put $path $text
    }
    if($family -in @('G16','G22')){Copy-Item -LiteralPath (Join-Path $tool 'firmware\common\isp_small\post_build.ps1') -Destination (Join-Path $boot 'post_build.ps1')}
    foreach($role in 'boot','app','transfer'){
        $name=switch($role){boot {$newBoot} app {$newBoot+'_app'} transfer {$newTransfer}}
        $folder=Join-Path $DeviceRoot $name;$project=Join-Path $folder ($name+'.mtpj')
        if($family -eq 'F24'){
            & (Join-Path $PSScriptRoot 'prepare_rl78_serial_scfg.ps1') -Project $folder -Initialize -Generate
            & (Join-Path $boot 'tools\prepare_projects.ps1') -Kind $role -Project $folder
        }else{
            [xml]$xml=[IO.File]::ReadAllText($project)
            $node=$xml.SelectSingleNode('//COptionD-0');$node.InnerText+="`nISP_$($kind.ToUpperInvariant())`n"
            $group=$xml.SelectSingleNode('//Class[Instance[Type="File"]]')
            $prefix=if($role -eq 'boot'){''}else{'..\'+$newBoot+'\'}
            foreach($file in $(if($kind -eq 'I2C'){@('isp_i2c_mailbox.c','isp_i2c.c')}else{@('isp_xmodem.c')})){
                $node=$group.SelectSingleNode('Instance[Type="File"]').CloneNode($true);$node.SetAttribute('Guid',[guid]::NewGuid().ToString())
                $node.SelectSingleNode('Name').InnerText=$file;$node.SelectSingleNode('RelativePath').InnerText=$prefix+'isp\'+$file
                [void]$group.AppendChild($node)
            }
            if($family -in @('G16','G22')){$xml.SelectSingleNode('//PostBuildCommands-0').InnerText='powershell.exe -NoProfile -ExecutionPolicy Bypass -File "'+$prefix+'post_build.ps1" -Kind '+$role+' -Project "."'}
            if($family -eq 'G22' -and $kind -eq 'Xmodem' -and $role -eq 'boot'){
                # The XMODEM input union needs32 more bytes below the copied RFD.
                $node=$xml.SelectSingleNode('//LinkOptionStart-0')
                $node.InnerText=$node.InnerText.Replace('/FF400,','/FF420,')
            }
            $writer=[Xml.XmlWriter]::Create($project);try{$xml.Save($writer)}finally{$writer.Dispose()}
            if($family -in @('G16','G22')){& (Join-Path $PSScriptRoot 'prepare_rl78_serial_scfg.ps1') -Project $folder -Initialize -Generate}
        }
        Write-Host ('Created '+$name+'; target build/model/hardware not implied.')
    }
}}
