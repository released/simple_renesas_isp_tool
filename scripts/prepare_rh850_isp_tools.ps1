param([Parameter(Mandatory=$true)][string]$BootProject,[ValidateSet('UART','I2C','Xmodem')][string]$Bus)
$ErrorActionPreference='Stop'
$boot=(Resolve-Path -LiteralPath $BootProject).Path
$parent=Split-Path -Parent $boot
$source=Join-Path $parent 'Sample_Project_RH850_S1_Boot_loader_CAN\tools\device_build.ps1'
$utf8=New-Object Text.UTF8Encoding($false)
function Put([string]$Path,[string]$Text){[IO.File]::WriteAllText($Path,$Text,$utf8)}
$text=[IO.File]::ReadAllText($source)
$text=$text.Replace("[ValidateSet('boot','app')]","[ValidateSet('boot','app','transfer')]")
$text=$text.Replace('$isApp=$Kind -eq ''app''',"`$isApp=`$Kind -eq 'app'`n`$isTransfer=`$Kind -eq 'transfer'")
$text=$text.Replace('$prefix=if($isApp)', '$prefix=if($isApp -or $isTransfer)')
$text=[regex]::Replace($text,'(?m)^if\(!\$isApp\)\{\r?\n(?=    \$begin=Map-Section)',"if(!`$isApp -and !`$isTransfer){`n")
$functions=@'
function Transport-Define([string]$Key){
    $h=[IO.File]::ReadAllText((Join-Path $boot 'common\isp_transport_config.h'))
    $m=[regex]::Match($h,'(?m)^#define\s+'+[regex]::Escape($Key)+'\s+(\([^\r\n)]+\))')
    if(!$m.Success){throw ('Missing literal serial setting '+$Key)}
    return ConvertFrom-AppUnsignedLiteral $m.Groups[1].Value
}
function Get-SerialIrq {
    if((Transport-Define ISP_CONTRACT_ID) -eq 2){return @{240='IspI2cTxIsr';241='IspI2cEventIsr';242='IspI2cRxIsr';243='IspI2cEndIsr';84='r_Config_OSTM0_interrupt'}}
    return @{122='IspUartRxIsr';123='IspUartErrorIsr';84='r_Config_OSTM0_interrupt'}
}
'@
$text=$text.Replace('function Read-Irq {',$functions+"`nfunction Read-Irq {")
$text=$text.Replace("if(`$Action -in @('prepare','pre')){", "if(`$Action -in @('prepare','pre')){`n    & (Join-Path `$boot 'tools\validate_serial_settings.ps1') -BootProject `$boot -Project `$Project")
$text=$text.Replace("if(`$mapping[23] -cne 'Device_CanRxIsr' -or `$mapping[84] -cne 'r_Config_OSTM0_interrupt'){throw 'CAN RX23 and OSTM84 are required by this App runtime.'}",
    'foreach($required in (Get-SerialIrq).GetEnumerator()){if($mapping[$required.Key] -cne $required.Value){throw ''Required serial/timer IRQ mapping is missing.''}}')
$text=$text.Replace("@{23='Device_CanRxIsr';84='r_Config_OSTM0_interrupt'}",'(Get-SerialIrq)')
$text=$text.Replace('Read-AppImageConfig $Project 0x8501','Read-AppImageConfig $Project 0x8501 (Transport-Define ISP_UPDATE_TRANSPORT)')
$begin=$text.IndexOf("    `$sources=@('device_image.c'")
$end=$text.IndexOf('    $parent=[string]$xml.SelectSingleNode', $begin)
if($begin -lt 0 -or $end -lt 0){throw 'CAN helper source ownership anchor changed.'}
$newSources=@'
    foreach($old in @($files.Instance|Where-Object {$_.Type -eq 'File'})){[void]$files.RemoveChild($old)}
    $sources=@('boot.asm','cstart.asm',
        'src\smc_gen\Config_OSTM0\Config_OSTM0.c',
        'src\smc_gen\Config_TAUJ0_0\Config_TAUJ0_0.c',
        'src\smc_gen\Config_TAUJ0_0\Config_TAUJ0_0_user.c',
        'src\smc_gen\general\r_cg_cgc_user.c',
        'src\smc_gen\general\r_smc_interrupt.c','src\smc_gen\r_pincfg\Pin.c',
        'src\smc_gen\Config_UART0\Config_UART0.c','src\smc_gen\Config_UART0\Config_UART0_user.c',
        'peripherals\port\r_port.c','generated_projection\device_vectors.c','generated_projection\device_timer.c')
    $core=@('device_image.c','image_metadata.c','image_metadata_profile.c','device_read.c','device_cpu.asm',
        'isp_rh850_runtime.c','isp_protocol.c','isp_device.c')
    if((Transport-Define ISP_CONTRACT_ID) -eq 2){
        $core+=@('isp_rh850_i2c.c','isp_i2c_mailbox.c');$driver='Config_RIIC1'
    }else{
        $core+='isp_rh850_uart.c';$driver='Config_UART1'
        if((Transport-Define ISP_CONTRACT_ID) -eq 3){$core+='isp_xmodem.c'}
    }
    $sources+=@('src\smc_gen\'+$driver+'\'+$driver+'.c')
    $sources+=@($core|ForEach-Object {$prefix+'common\'+$_})
    if(!$isApp -and !$isTransfer){
        $sources+=@('common\device_flash.c','common\flash_geometry.c')
        $sources+=Get-ChildItem (Join-Path $boot 'FCL\lib') -File|Where-Object {$_.Extension -in @('.c','.asm')}|ForEach-Object {'FCL\lib\'+$_.Name}
        $sources+=@('FCL_user\fcl_descriptor.c','FCL_user\fcl_user.c')
    }
    if(!$isApp){$sources+=@('generated_projection\evm_clock.c','src\smc_gen\general\r_cg_systeminit.c')}
    if($isApp){$sources+=@('generated_projection\app_system.c','app_entry.asm','app_image_config.h','app_irq_handlers.h','app_runtime_log.h')}
'@
$text=$text.Substring(0,$begin)+$newSources+"`n"+$text.Substring($end)
$oldIncludes='$includes.InnerText=((($includes.InnerText.Trim()+"`n."+"`ngenerated_projection`n"+$prefix+"common`n"+$prefix+"FCL`n"+$prefix+"FCL\lib`n"+$prefix+"FCL_user") -split ''\r?\n'') | Select-Object -Unique) -join "`n"'
$newIncludes='$includes.InnerText=(@(''.'',''generated_projection'',''src\smc_gen\general'',''src\smc_gen\r_pincfg'',''src\smc_gen\Config_UART0'',''src\smc_gen\Config_TAUJ0_0'',''src\smc_gen\Config_OSTM0'',(''src\smc_gen\''+$driver),''peripherals\port'',($prefix+''common''),($prefix+''FCL''),($prefix+''FCL\lib''),($prefix+''FCL_user''))) -join "`n"'
if(!$text.Contains($oldIncludes)){throw 'CAN include anchor changed.'}
$text=$text.Replace($oldIncludes,$newIncludes)
$text=$text.Replace("if(`$isApp){'RH850_DEVICE_APP'}else{'RH850_DEVICE_BOOT'}", "if(`$isApp){`"RH850_DEVICE_APP``nISP_DEVICE_APP`"}elseif(`$isTransfer){'ISP_DEVICE_TRANSFER'}else{`"RH850_DEVICE_BOOT``nISP_DEVICE_BOOT`"}")
$text=$text.Replace('if($fd){$xml.SelectSingleNode(''//COptionD-0'').InnerText+="`nRH850_CAN_FD=1"}',
    'if((Transport-Define ISP_CONTRACT_ID) -eq 2){$xml.SelectSingleNode(''//COptionD-0'').InnerText+="`nISP_I2C"}elseif((Transport-Define ISP_CONTRACT_ID) -eq 3){$xml.SelectSingleNode(''//COptionD-0'').InnerText+="`nISP_XMODEM"}')
$text=$text.Replace("if(!`$isApp){`$rom+=", "if(!`$isApp -and !`$isTransfer){`$rom+=")
$text=$text.Replace("`$(if(!`$isApp){',R_FCL_DATA.bss'})", "`$(if(!`$isApp -and !`$isTransfer){',R_FCL_DATA.bss'})")
$text=$text.Replace("if(!`$isApp){`$layout+=',FCL_RESERVED.bss/FEDF8000'}", "if(!`$isApp -and !`$isTransfer){`$layout+=',FCL_RESERVED.bss/FEDF8000'}")
$text=$text.Replace("if(!`$isApp){`r`n    `$begin=Map-Section", "if(!`$isApp -and !`$isTransfer){`r`n    `$begin=Map-Section")
$text=$text.Replace("Write-Hex (Join-Path `$out 'boot0000_FFFF.hex') `$bytes 0", "Write-Hex (Join-Path `$out `$(if(`$isTransfer){`$name+'.hex'}else{'boot0000_FFFF.hex'})) `$bytes 0")
$text=$text.Replace('0x01050001,','(0x01010000 -bor (Transport-Define ISP_BOOT_PROTOCOL_ID)),')
$text=$text.Replace("((Image-Define IMAGE_TARGET_ID) -bor (`$(if(`$fd){2}else{1}) -shl 16))",'(Image-Define IMAGE_TARGET_ID)')
$text=$text.Replace('(Image-Define IMAGE_RESERVED_SIZE),(Image-Define IMAGE_ERASE_BLOCK_SIZE),4,$(if($fd){2}else{0}),0x700,0x701,',
    '(Image-Define IMAGE_RESERVED_SIZE),(Image-Define IMAGE_ERASE_BLOCK_SIZE),(Transport-Define ISP_LINK_PARAMETER_0),(Transport-Define ISP_LINK_PARAMETER_1),0,0,')
$text=$text.Replace('(Image-Define IMAGE_BOOT_VERSION),0,$id.VersionControl,0,0)',
    '(Image-Define IMAGE_BOOT_VERSION),(Transport-Define ISP_POLICY_UART_BAUD),$id.VersionControl,0,0)')
Put (Join-Path $boot 'tools\device_build.ps1') $text
Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) 'firmware\common\isp_rh850\validate_serial_settings.ps1') -Destination (Join-Path $boot 'tools\validate_serial_settings.ps1')
$readerPath=Join-Path $boot 'tools\app_config.ps1'
$reader=[IO.File]::ReadAllText($readerPath)
$reader=$reader.Replace('[uint32]$ExpectedTargetId)', '[uint32]$ExpectedTargetId,[uint32]$ExpectedTransport=1)')
$reader=$reader.Replace('$values.APP_IMAGE_UPDATE_TRANSPORTS -ne 1','$values.APP_IMAGE_UPDATE_TRANSPORTS -ne $ExpectedTransport').Replace('CAN DEVICE_APP','selected ISP DEVICE_APP')
Put $readerPath $reader
$build=@'
param([ValidateSet('boot','app','transfer','all')][string]$Kind='all',[string]$Project='', [string]$Compiler='')
$ErrorActionPreference='Stop'
$boot=Split-Path -Parent $PSScriptRoot;$base=Split-Path -Leaf $boot
$bus=($base -split '_')[-1]
if(!$Compiler){$Compiler=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\CS+\CC\CubeSuite+.exe'}
if(!(Test-Path -LiteralPath $Compiler)){throw 'Installed CS+ is required;use -Compiler to select CubeSuite+.exe.'}
$items=if($Kind -eq 'all'){@('boot','app','transfer')}else{@($Kind)}
foreach($item in $items){
    $folder=if($Project -and $Kind -ne 'all'){$Project}else{switch($item){
        boot{$boot}app{Join-Path (Split-Path -Parent $boot) ($base+'_app')}
        transfer{Join-Path (Split-Path -Parent $boot) ('Sample_Project_RH850_S1_'+$(if($bus -eq 'I2C'){'I2C_Slave_Transfer'}else{$bus+'_Transfer'}))}
    }}
    $folder=(Resolve-Path -LiteralPath $folder).Path;$name=Split-Path -Leaf $folder
    & (Join-Path $PSScriptRoot 'device_build.ps1') -Action prepare -Project $folder -Kind $item
    $logs=Join-Path $folder 'tmp\build';[IO.Directory]::CreateDirectory($logs)|Out-Null
    $log=Join-Path $logs ($item+'_rebuild.log')
    $p=Start-Process -FilePath $Compiler -ArgumentList ('/br "'+(Join-Path $folder ($name+'.mtpj'))+'"') -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr')
    $result=[IO.File]::ReadAllText($log);$result -split '\r?\n'|Select-Object -Last 16|Write-Host
    if(($null -ne $p.ExitCode -and $p.ExitCode -ne 0) -or $result -notmatch 'Build ended\(Error:0, Warning:\d+\)' -or $result -match 'Failed:[1-9]'){throw ('RH850 serial build/post-build failed: '+$log)}
    & (Join-Path $PSScriptRoot 'device_build.ps1') -Action post -Project $folder -Kind $item
}
Write-Host 'PASS selected RH850 serial linked-image/build guards. Hardware NOT RUN.'
'@
Put (Join-Path $boot 'tools\build.ps1') $build
Write-Host ('Prepared local RH850 '+$Bus+' build ownership and image guards.')
