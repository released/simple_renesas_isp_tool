param(
    [Parameter(Mandatory=$true)][string]$DeviceRoot,
    [Parameter(Mandatory=$true)][string]$G16RfSp,
    [Parameter(Mandatory=$true)][string]$G22Rfd,
    [Parameter(Mandatory=$true)][string]$G16DeviceHeader,
    [Parameter(Mandatory=$true)][string]$G22DeviceHeader,
    [ValidateSet('G16','G22')][string[]]$Mcu=@('G16','G22')
)
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path
$utf8=New-Object Text.UTF8Encoding($false)
function Put([string]$Path,[string]$Text) {
    [IO.File]::WriteAllText($Path,$Text,$utf8)
}
function Clone([string]$From,[string]$To) {Copy-Item -LiteralPath $From -Destination $To}
function Macro([string]$Text,[string]$Key,[string]$Value) {
    $pattern='(?m)^(#define\s+'+[regex]::Escape($Key)+'\s+)\([^\r\n]+?\)'
    if(([regex]::Matches($Text,$pattern)).Count -ne 1){throw ('Expected unique macro '+$Key)}
    return [regex]::Replace($Text,$pattern,[Text.RegularExpressions.MatchEvaluator]{param($m)$m.Groups[1].Value+'('+$Value+')'})
}
function Words([string]$Text,[int]$Count) {
    $bytes=New-Object byte[] ($Count*4);[Text.Encoding]::ASCII.GetBytes($Text).CopyTo($bytes,0)
    for($i=0;$i -lt $Count;$i++){[BitConverter]::ToUInt32($bytes,$i*4)}
}
$base=Join-Path $DeviceRoot 'RL78_F13_ISP_Boot_loader_UART'
$baseApp=$base+'_app'
foreach($family in $Mcu) {
    foreach($name in @(('RL78_'+$family+'_ISP_Boot_loader_UART'),('RL78_'+$family+'_ISP_Boot_loader_UART_app'),('RL78_'+$family+'_UART_Transfer'))) {
        $destination=Join-Path $DeviceRoot $name
        if(Test-Path -LiteralPath $destination){throw ('Scaffold already exists; never overwrite '+$destination)}
    }
}
foreach($family in $Mcu) {
    $bootName='RL78_'+$family+'_ISP_Boot_loader_UART';$boot=Join-Path $DeviceRoot $bootName
    $part=if($family -eq 'G16'){'R5F121BCAFP'}else{'R7F102GGE2DFB'}
    $device=if($family -eq 'G16'){'R5F121BCxFP'}else{'R7F102GGExFB'}
    $dvf=if($family -eq 'G16'){'DR5F121BC.DVF, V1.30'}else{'DR7F102GGE.DVF, V1.20'}
    $flash=if($family -eq 'G16'){0x8000}else{0x10000}
    $ram=if($family -eq 'G16'){0xFF700}else{0xFEF00}
    $irq=if($family -eq 'G16'){0x31563647}else{0x31563247}
    $timer=if($family -eq 'G16'){0x1C}else{0x2E}
    $rx=if($family -eq 'G16'){0x20}else{0x26}
    $target=if($family -eq 'G16'){0x16B}else{0x22B}
    [IO.Directory]::CreateDirectory((Join-Path $boot 'isp'))|Out-Null
    foreach($file in @('AGENTS.md','.gitattributes','.gitignore','image_metadata.c','image_metadata.h','image_metadata_profile.c',
            'sw_crc32.c','sw_crc32.h','app_config.ps1','app_build_report.ps1','verify_boot_budget.ps1','ftable.inc','stkinit.asm')) {
        Clone (Join-Path $base $file) (Join-Path $boot $file)
    }
    foreach($file in @('isp_protocol.c','isp_protocol.h','isp_device.c','isp_device.h','isp_fixed_profile.h','isp_uart_timing.h','isp_bus_contract.h')) {
        Clone (Join-Path $tool ('firmware\common\isp_device\'+$file)) (Join-Path $boot ('isp\'+$file))
    }
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $tool 'firmware\common\isp_small') -File) {
        if($file.Name -ne 'g16_flash.c' -and $file.Extension -in @('.c','.h')){Clone $file.FullName (Join-Path $boot ('isp\'+$file.Name))}
    }
    Clone (Join-Path $tool 'firmware\common\isp_small\verify_ram_budget.ps1') (Join-Path $boot 'verify_ram_budget.ps1')
    Clone (Join-Path $tool 'firmware\common\isp_small\post_build.ps1') (Join-Path $boot 'post_build.ps1')
    if($family -eq 'G16') {
        Clone $G16DeviceHeader (Join-Path $boot 'iodefine.h')
        Copy-Item -LiteralPath $G16RfSp -Destination (Join-Path $boot 'RFSPRL78T01') -Recurse
        Clone (Join-Path $tool 'firmware\common\isp_small\g16_flash.c') (Join-Path $boot 'device_flash.c')
    } else {
        Clone $G22DeviceHeader (Join-Path $boot 'iodefine.h')
        Copy-Item -LiteralPath $G22Rfd -Destination (Join-Path $boot 'RFDRL78T01') -Recurse
        $text=[IO.File]::ReadAllText((Join-Path $DeviceRoot 'RL78_F24_ISP_Boot_loader_UART\f24_flash.c'))
        $text=$text.Replace('#include "device_board.h"','#include "isp_rl78_board.h"').Replace('R_RFD_Init(40U)','R_RFD_Init(32U)')
        $text=$text.Replace('uint32_t','unsigned long').Replace('uint16_t','unsigned short').Replace('uint8_t','unsigned char')
        $text=$text.Replace('R_RFD_ENUM_FLASH_MODE_CODE_TO_NONPROGRAMMABLE','R_RFD_ENUM_FLASH_MODE_UNPROGRAMMABLE')
        Put (Join-Path $boot 'device_flash.c') $text
    }
    $image=[IO.File]::ReadAllText((Join-Path $base 'image_metadata_config.h'))
    foreach($pair in @(@('IMAGE_TARGET_ID',('0x{0:X4}U' -f $target)),@('IMAGE_APPLICATION_CAPACITY',('0x{0:X5}UL' -f ($flash-0x2400))),
            @('IMAGE_CODE_FLASH_SIZE',('0x{0:X5}UL' -f $flash)),@('IMAGE_IRQ_ABI',('0x{0:X8}UL' -f $irq)))) {
        $image=Macro $image $pair[0] $pair[1]
    }
    $partWords=@(Words $part 4);$familyWords=@(Words ('RL78/'+$family) 3)
    for($i=0;$i -lt 4;$i++){$image=Macro $image ('IMAGE_MCU_PART_'+$i) ('0x{0:X8}UL' -f $partWords[$i])}
    for($i=0;$i -lt 3;$i++){$image=Macro $image ('IMAGE_MCU_FAMILY_'+$i) ('0x{0:X8}UL' -f $familyWords[$i])}
    $image=$image.Replace('F13','MCU').Replace('0x1FBFF',('0x{0:X5}' -f ($flash-1025))).Replace('119 KiB',(($flash-0x2400)/1024).ToString()+' KiB')
    $image=$image.Replace('Full 120-KiB engineering profiles','Full-capacity engineering profiles')
    Put (Join-Path $boot 'image_metadata_config.h') $image
    $handoff=[IO.File]::ReadAllText((Join-Path $base 'boot_app_handoff.h'))
    $handoff=Macro $handoff 'BOOT_APP_SHARED_RAM_ADDRESS' ('0x{0:X8}UL' -f ($ram+16))
    Put (Join-Path $boot 'boot_app_handoff.h') $handoff
    $transport=[IO.File]::ReadAllText((Join-Path $base 'isp_transport_config.h'))
    $transport=Macro $transport 'ISP_UART_RX_VECTOR' ('0x{0:X2}U' -f $rx)
    $transport=$transport.Replace('RX vector 0x40','RX vector '+('0x{0:X2}' -f $rx))
    Put (Join-Path $boot 'isp_transport_config.h') $transport
    $partBytes=(@([Text.Encoding]::ASCII.GetBytes($part) | ForEach-Object {"'"+[char]$_+"'"})+@(0)*(16-$part.Length)) -join ','
    $familyBytes=(@([Text.Encoding]::ASCII.GetBytes('RL78/'+$family) | ForEach-Object {"'"+[char]$_+"'"})+@(0)*4) -join ','
    $hoco=if($family -eq 'G16'){'HOCO16'}else{'HOCO32'}
    Put (Join-Path $boot 'isp_board_config.h') ("#ifndef ISP_BOARD_CONFIG_H`n#define ISP_BOARD_CONFIG_H`n"+
        "/* Board: $part, $hoco. RX/timer vector sources: iodefine.h. */`n"+
        "#define ISP_MCU_PART_BYTES {$partBytes}`n#define ISP_MCU_FAMILY_BYTES {$familyBytes}`n"+
        "#define ISP_TIMER_VECTOR (0x$('{0:X2}' -f $timer)U)`n#endif`n")
    $generator=[IO.File]::ReadAllText((Join-Path $base 'generate_irq_contract.ps1'))
    $generator=$generator.Replace('RL78_F13_ISP_Boot_loader_UART',$bootName).Replace('F13',$family).Replace('f13',$family.ToLowerInvariant())
    $generator=$generator.Replace('0xFF710',('0x{0:X5}' -f ($ram+16))).Replace('0x1E*2',('0x{0:X2}*2' -f $timer)).Replace('0x2080',('0x{0:X4}' -f (0x2000+$rx*2)))
    Put (Join-Path $boot 'generate_irq_contract.ps1') $generator
    $budget=[IO.File]::ReadAllText((Join-Path $base 'verify_boot_budget.ps1')).Replace('RL78_F13_ISP_Boot_loader_UART',$bootName)
    Put (Join-Path $boot 'verify_boot_budget.ps1') $budget
    foreach($role in 'boot','app','transfer') {
        $name=switch($role){boot {$bootName} app {$bootName+'_app'} transfer {'RL78_'+$family+'_UART_Transfer'}}
        $folder=Join-Path $DeviceRoot $name
        [IO.Directory]::CreateDirectory($folder)|Out-Null
        if($role -ne 'boot') {
            foreach($file in @('AGENTS.md','.gitattributes','.gitignore','stkinit.asm')){Clone (Join-Path $base $file) (Join-Path $folder $file)}
            Clone (Join-Path $boot 'iodefine.h') (Join-Path $folder 'iodefine.h')
        }
        $startup=[IO.File]::ReadAllText((Join-Path $(if($role -eq 'app'){$baseApp}else{$base}) 'cstart.asm'))
        $startup=$startup.Replace('.DS     0x500','.DS     0x200').Replace('$IF (__RENESAS_VERSION__ < 0x01010000)','$IF (1)')
        $startup=$startup.Replace('__STACK_ADDR_START','_stacktop').Replace('__STACK_ADDR_END','_stackend')
        if($role -eq 'app'){$startup=$startup.Replace('_start'+[char]9+'.VECTOR'+[char]9+'0','; Physical reset belongs to Boot.')}
        if($family -eq 'G22' -and $role -eq 'boot') {
            $declare='';$copy=''
            foreach($segment in @('RFD_DATA_n','RFD_CMN_f','RFD_CF_f','DRAM_f')) {
                $type=if($segment -eq 'RFD_DATA_n'){'DATA'}else{'TEXTF'}
                $declare+=".SECTION ${segment}R, $type`n"
                $copy+="    MOV ES,#HIGHW(STARTOF($segment))`n    MOVW HL,#LOWW(STARTOF($segment))`n"+
                    "    MOVW DE,#LOWW(STARTOF(${segment}R))`n    MOVW BC,#LOWW(SIZEOF($segment))`n"+
                    ".L_COPY_${segment}:`n    MOVW AX,BC`n    CLRW AX`n    CMPW AX,BC`n    BZ `$"+".L_DONE_$segment`n"+
                    "    MOV A,ES:[HL]`n    MOV [DE],A`n    INCW HL`n    INCW DE`n    DECW BC`n    BR `$"+".L_COPY_$segment`n.L_DONE_${segment}:`n"
            }
            $startup=$startup.Replace('.SECTION .dataR, DATA',$declare+'.SECTION .dataR, DATA')
            $startup=$startup.Replace('CALL'+[char]9+'!!_boot_main',$copy+"    CALL !!_boot_main")
        }
        Put (Join-Path $folder 'cstart.asm') $startup
        if($role -eq 'app') {
            foreach($file in @('app_main.c','app_image_config.h','app_irq_handlers.h','image_metadata_config.h','ftable_RL78_F13.asm','generate_update_image.ps1')) {
                $text=[IO.File]::ReadAllText((Join-Path $baseApp $file))
                $text=$text.Replace('RL78_F13_ISP_Boot_loader_UART',$bootName).Replace('0xF13BU',('0x{0:X4}U' -f $target))
                $text=$text.Replace('f13_app_',($family.ToLowerInvariant()+'_app_')).Replace('F13',$family)
                if($file -eq 'app_irq_handlers.h'){$text=$text.Replace('0x1E',('0x{0:X2}' -f $timer)).Replace('0x40',('0x{0:X2}' -f $rx))}
                $text=$text.Replace('flash2000_1FFFF',('flash2000_{0:X}' -f ($flash-1)))
                Put (Join-Path $folder ($file.Replace('F13',$family))) $text
            }
        }
        $template=Join-Path $(if($role -eq 'app'){$baseApp}else{$base}) $(if($role -eq 'app'){'RL78_F13_ISP_Boot_loader_UART_app.mtpj'}else{'RL78_F13_ISP_Boot_loader_UART.mtpj'})
        $raw=[IO.File]::ReadAllText($template).Replace('RL78_F13_ISP_Boot_loader_UART_app',$bootName+'_app').Replace('RL78_F13_ISP_Boot_loader_UART',$name)
        $raw=$raw.Replace('R5F10BMG',$device).Replace('DR5F10BMG.DVF, V1.11',$dvf)
        [xml]$xml=$raw
        $group=$xml.SelectSingleNode('//Class[Instance[Type="File"]]')
        $parent=[string]$group.SelectSingleNode('Instance[Type="File"]/ParentItem').InnerText
        foreach($node in @($group.SelectNodes('Instance[Type="File"]'))){[void]$group.RemoveChild($node)}
        $prefix=if($role -eq 'boot'){''}else{'..\'+$bootName+'\'}
        $files=@('cstart.asm','stkinit.asm')+@('isp\isp_protocol.c','isp\isp_device.c','isp\isp_runtime.c','isp\isp_board.c','sw_crc32.c') | ForEach-Object {if($_ -in @('cstart.asm','stkinit.asm')){$_}else{$prefix+$_}}
        if($role -eq 'app'){$files+=@('app_main.c',('ftable_RL78_'+$family+'.asm'))}
        else{$files+=@(($prefix+'isp\isp_entry.c'),($family.ToLowerInvariant()+'_boot_vectors.asm'))}
        if($role -ne 'transfer'){$files+=@(($prefix+'image_metadata.c'),($prefix+'image_metadata_profile.c'))}
        if($role -eq 'boot') {
            $files+='device_flash.c'
            $vendor=if($family -eq 'G16'){'RFSPRL78T01'}else{'RFDRL78T01'}
            $api=if($family -eq 'G16'){'rfsp'}else{'rfd'}
            $files+=@(('source\common\r_'+$api+'_common_api.c'),('source\common\r_'+$api+'_common_control_api.c'),('source\codeflash\r_'+$api+'_code_flash_api.c')) | ForEach-Object {$vendor+'\'+$_}
            if($family -eq 'G22'){$files+=@(($vendor+'\source\common\r_rfd_common_get_api.c'),($vendor+'\userown\r_rfd_common_userown.c'))}
        }
        foreach($file in $files) {
            $node=$xml.CreateElement('Instance');$node.SetAttribute('Guid',[guid]::NewGuid().ToString())
            foreach($pair in @(@('Name',[IO.Path]::GetFileName($file)),@('Type','File'),@('RelativePath',$file),@('TreeImageGuid','941832c1-fc3b-4e1b-94e8-01ea17128b42'),@('ParentItem',$parent))){
                $child=$xml.CreateElement($pair[0]);$child.InnerText=$pair[1];[void]$node.AppendChild($child)
            }
            [void]$group.AppendChild($node)
        }
        function Set-Option([string]$Key,[string]$Value){$n=$xml.SelectSingleNode('//'+$Key);if(!$n){throw ('Missing MTPJ option '+$Key)};$n.InnerText=$Value}
        $include=".`n${prefix}isp`n"+$(if($prefix){$prefix+"`n"}else{''})
        $include+=if($family -eq 'G16'){"${prefix}RFSPRL78T01\include`n${prefix}RFSPRL78T01\include\rfsp`n"}else{"${prefix}RFDRL78T01\include`n${prefix}RFDRL78T01\include\rfd`n"}
        Set-Option 'COptionI-0' $include;Set-Option 'AsmOptionI-0' $include
        Set-Option 'COptionD-0' ("ISP_SMALL_TARGET`nISP_MCU_$family`nISP_DEVICE_"+$role.ToUpperInvariant()+"`n")
        Set-Option 'GeneralOptionCpu-0' $(if($family -eq 'G16'){'S2'}else{'S3'})
        Set-Option 'COptionPreinclude-0' '';Set-Option 'LinkOptionAutoSectionLayout-0' 'False'
        Set-Option 'LinkOptionDebugMonitor-0' ('{0:X}-{1:X}' -f ($flash-256),($flash-1))
        Set-Option 'GeneratorOptionCurrentDevice-0' $dvf
        Set-Option 'LinkOptionUserOptByteValue-0' $(if($family -eq 'G16'){'EFF7F8'}else{'EFFFE8'})
        Set-Option 'LinkOptionSecurityOptByteValue-0' '85'
        $code='.const,.btext,.btextf,.text,.RLIB,.SLIB,.textf,.constf,.data,.sdata,.init_array'
        if($family -eq 'G22' -and $role -eq 'boot'){$code+=',RFD_DATA_n,RFD_CMN_f,RFD_CF_f,DRAM_f'}
        $layout=$code+$(if($role -eq 'app'){'/02200'}else{'/000D8'})
        $layout+=',.dataR,.bss/'+('{0:X5}' -f ($ram+32))
        if($family -eq 'G22' -and $role -eq 'boot'){$layout+=',RFD_DATA_nR,RFD_CMN_fR,RFD_CF_fR,DRAM_fR/FF400'}
        $layout+=',.boot_info/'+('{0:X5}' -f $ram)+',.stack_bss/FFC20,.sdataR,.sbss/FFE20'
        Set-Option 'LinkOptionStart-0' $layout
        $rom=".data=.dataR`n.sdata=.sdataR`n"
        if($family -eq 'G22' -and $role -eq 'boot'){foreach($seg in @('RFD_DATA_n','RFD_CMN_f','RFD_CF_f','DRAM_f')){$rom+="$seg=${seg}R`n"}}
        Set-Option 'LinkOptionRom-0' $rom
        Set-Option 'HexOptionOutputFileName-0' '%ProjectName%.hex'
        $pre='powershell.exe -NoProfile -ExecutionPolicy Bypass -File "'+$prefix+'generate_irq_contract.ps1" -Mode '+$(if($role -eq 'app'){'App -AppProject "."'}else{'Boot -OutputProject "."'})
        Set-Option 'PreBuildCommands-0' $pre
        if($role -eq 'app') {
            Set-Option 'HexOptionDivisionOutputFile-0' ('%BuildModeName%\flash2000_{0:X}.hex=2000-{0:X}' -f ($flash-1))
            Set-Option 'PostBuildCommands-0' 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "generate_update_image.ps1"'
        }else{
            Set-Option 'HexOptionDivisionOutputFile-0' $(if($role -eq 'boot'){'%BuildModeName%\boot0000_1FFF.hex=0000-1FFF'}else{''})
        Set-Option 'PostBuildCommands-0' ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "'+$prefix+'verify_boot_budget.ps1" -MapPath "DefaultBuild\'+$name+'.map"')
        }
        Set-Option 'PostBuildCommands-0' ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "'+$prefix+'post_build.ps1" -Kind '+$role+' -Project "."')
        foreach($n in $xml.SelectNodes('//ProjectGuid|//ActiveProjectGuid')){$n.InnerText=[guid]::NewGuid().ToString()}
        $settings=New-Object Xml.XmlWriterSettings;$settings.Encoding=$utf8;$settings.OmitXmlDeclaration=$true
        $writer=[Xml.XmlWriter]::Create((Join-Path $folder ($name+'.mtpj')),$settings)
        try{$xml.Save($writer)}finally{$writer.Dispose()}
        $wrapper=if($role -eq 'boot'){
            [IO.File]::ReadAllText((Join-Path $base 'build.ps1')).Replace('(F13|F24)','(F13|F24|G16|G22)')
        }else{[IO.File]::ReadAllText((Join-Path $(if($role -eq 'app'){$baseApp}else{Join-Path $DeviceRoot 'RL78_F13_UART_Transfer'}) 'build.ps1')).Replace('RL78_F13_ISP_Boot_loader_UART',$bootName).Replace('(F13|F24)','(F13|F24|G16|G22)')}
        Put (Join-Path $folder 'build.ps1') $wrapper
        Put (Join-Path $folder 'README.md') ("# $name`n`n$part, UART1 binary ISP115200/8-N-1, separate UART0 text Log.`n`n"+
            "Keep the matching $bootName project as a sibling. Edit App release/IRQ inputs in app_image_config.h and app_irq_handlers.h.`n`n"+
            'Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1` with the installed CS+/CC-RL environment.'+"`n`nOffline build/model checks are separate from board UART/Flash acceptance.`n")
    }
    foreach($name in @($bootName,($bootName+'_app'),('RL78_'+$family+'_UART_Transfer'))){
        & (Join-Path $PSScriptRoot 'prepare_rl78_serial_scfg.ps1') -Project (Join-Path $DeviceRoot $name) -Initialize -Generate
    }
    Write-Host ('Created independent '+$family+' UART Boot/App/Transfer. Generate SC before Rebuild; hardware NOT YET RUN.')
}
