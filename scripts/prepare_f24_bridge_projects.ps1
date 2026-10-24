param([ValidateSet('all','boot','app')][string]$Kind='all')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$board=Join-Path $repo 'firmware\f24_board'
$generated=Join-Path $board 'src\smc_gen'
. (Join-Path $PSScriptRoot 'f24_app_contract.ps1')
$layout=Read-F24MetadataConfig
$start=$layout.APP_START
$handoff=Read-F24HandoffConfig
$dispatch=($handoff.DISPATCH_ADDRESS-band0xFFFF).ToString('X4')
$appOwner=$handoff.APP_OWNER.ToString('X2')
$header=[IO.File]::ReadAllText((Join-Path $generated 'r_bsp\mcu\rl78_f24\register_access\ccrl\iodefine.h'))
$vectors=@{}
foreach($m in [regex]::Matches($header,'#define\s+(INT\w+)\s+(0x[0-9A-Fa-f]+)')) {
    $vectors[$m.Groups[1].Value]=[Convert]::ToInt32($m.Groups[2].Value.Substring(2),16)
}
$sources=@(Get-ChildItem -LiteralPath $generated -Recurse -File |
    Where-Object { $_.Extension -in @('.c','.asm') } | Sort-Object FullName)
$utf8=New-Object Text.UTF8Encoding($false)
function WriteGenerated([string]$path,[string]$text) {
    if (!(Test-Path -LiteralPath $path) -or [IO.File]::ReadAllText($path) -cne $text) {
        [IO.File]::WriteAllText($path,$text,$utf8)
    }
}
foreach($mode in @('boot','app')) {
    if ($Kind -ne 'all' -and $Kind -ne $mode) { continue }
    $name='RL78_F24_Bridge_'+$mode
    $folder=Join-Path $repo ('firmware\'+$name)
    $projectPath=Join-Path $folder ($name+'.mtpj')
    # Each project owns its options/tree. No third linkable template is used.
    [xml]$project=[IO.File]::ReadAllText($projectPath)
    $fileClass=$project.CubeSuiteProject.Class[0]
    $projection=Join-Path $folder 'generated_projection'
    New-Item -ItemType Directory -Path $projection -Force | Out-Null
    $bootHandlers=@{}
    $expected=@{}
    foreach($path in @('..\common\bridge_image_metadata.h',
        '..\f24_bridge_common\image_metadata_profile.c','..\f24_bridge_common\image_metadata_profile.h',
        '..\f24_bridge_common\image_metadata_config.h','..\f24_bridge_common\boot_app_handoff.h')) {
        $expected[$path]=$true
    }
    if($mode -eq 'app') {
        $expected['app_image_config.h']=$true; $expected['app_irq_handlers.h']=$true
        foreach($path in @('..\common\bridge_uart.c','..\common\bridge_uart.h',
            '..\f24_board\f24_uart_backend.c')) { $expected[$path]=$true }
    }
    foreach($source in $sources) {
        $relative=$source.FullName.Substring($board.Length+1)
        if ($mode -eq 'boot' -and !($relative -match 'smc_gen\\r_bsp\\' -or
            $relative -match 'smc_gen\\Config_(PORT|UART0|TAU0_1)\\' -or
            $relative -match 'r_cg_(sau|tau)_common.c$')) { continue }
        $target='..\f24_board\'+$relative
        $text=[IO.File]::ReadAllText($source.FullName)
        $changed=$false
        if ($source.Extension -eq '.c') {
            foreach($m in [regex]::Matches($text,'#pragma interrupt (\w+)\(vect=(\w+)\)')) {
                $handler=$m.Groups[1].Value; $vector=$vectors[$m.Groups[2].Value]
                if ($null -eq $vector) { throw "Unknown interrupt: $($m.Value)" }
                if ($mode -eq 'boot') { $bootHandlers[$vector]=$handler }
                $text=$text.Replace($m.Value,'#pragma interrupt '+$handler)
                $text=[regex]::Replace($text,'\bstatic\s+void\s+__near\s+'+$handler,'void __near '+$handler)
                $changed=$true
            }
            if ($mode -eq 'app' -and $source.Name -eq 'r_rscanfd_rl78.c') {
                foreach($m in [regex]::Matches($text,'R_BSP_PRAGMA_STATIC_INTERRUPT\s*\((\w+),\s*CAN_(INT\w+)_VECT\)')) {
                    $text=$text.Replace($m.Value,'#pragma interrupt '+$m.Groups[1].Value)
                }
                $text=$text.Replace('R_BSP_ATTRIB_STATIC_INTERRUPT void','void'); $changed=$true
            }
        }
        if ($changed) {
            $target='generated_projection\'+$source.Name
            $text="/* Generated projection; edit/regenerate canonical Smart Configurator source, then rerun prepare script. */`r`n"+$text
        }
        if ($source.Name -eq 'cstart.asm') {
            $text=$text.Replace('$IF (__RENESAS_VERSION__ < 0x01010000)  ; for CC-RL V1.00','$IF (1) ; Explicit bounded stack for split bridge')
            $text=$text.Replace('.DS     0x200','.DS     0x1000')
            $text=$text.Replace('__STACK_ADDR_START','_stacktop').Replace('__STACK_ADDR_END','_stackend')
            if ($mode -eq 'app') { $text=$text.Replace('_start  .VECTOR 0','; Reset is owned by the bridge boot project.') }
            else {
                $declarations=''; $copy="; Generated ROM-to-RAM copies. No flash mode switch before these copies.`r`n"
                foreach($s in @('RFD_DATA_n','RFD_CMN_f','RFD_CF_f','BRAM_f')) {
                    $declarations+=".SECTION ${s}R, $(if($s -eq 'RFD_DATA_n'){'DATA'}else{'TEXTF'})`r`n"
                    $copy+="    MOV ES,#HIGHW(STARTOF($s))`r`n    MOVW HL,#LOWW(STARTOF($s))`r`n    MOVW DE,#LOWW(STARTOF(${s}R))`r`n    MOVW BC,#LOWW(SIZEOF($s))`r`n"
                    $copy+=".L_COPY_$s`:`r`n    MOVW AX,BC`r`n    CMPW AX,#0`r`n    BZ `$.L_DONE_$s`r`n    MOV A,ES:[HL]`r`n    MOV [DE],A`r`n    INCW HL`r`n    INCW DE`r`n    DECW BC`r`n    BR `$.L_COPY_$s`r`n.L_DONE_$s`:`r`n"
                }
                $text=$text.Replace('.SECTION .dataR, DATA',$declarations+'.SECTION .dataR, DATA')
                $text=$text.Replace('    CALL    !!_bsp_init_hardware',$copy+"`r`n    CALL    !!_bsp_init_hardware")
            }
            $target='generated_projection\cstart.asm'; $changed=$true
        }
        if ($expected.ContainsKey($target)) { throw "Duplicate generated projection basename: $target" }
        $expected[$target]=$true
        if ($changed) { WriteGenerated (Join-Path $folder $target) $text }
    }
    # Retain existing entry order/GUIDs; reconcile only shared generated sources.
    foreach($node in @($fileClass.Instance | Where-Object {$_.Type -eq 'File'})) {
        $path=[string]$node.RelativePath
        if (($path.StartsWith('generated_projection\') -or $path.StartsWith('..\f24_board\src\smc_gen\')) -and
            [IO.Path]::GetExtension($path) -in @('.c','.asm') -and !$expected.ContainsKey($path)) {
            [void]$fileClass.RemoveChild($node)
        }
    }
    $registered=@{}
    foreach($node in @($fileClass.Instance | Where-Object {$_.Type -eq 'File'})) { $registered[[string]$node.RelativePath]=$true }
    foreach($path in @($expected.Keys | Sort-Object)) {
        if ($registered.ContainsKey($path)) { continue }
        $item=$project.CreateElement('Instance'); $item.SetAttribute('Guid',[guid]::NewGuid().ToString())
        foreach($pair in @{Name=[IO.Path]::GetFileName($path);Type='File';RelativePath=$path;
            TreeImageGuid='941832c1-fc3b-4e1b-94e8-01ea17128b42';ParentItem='76f247c0-0a47-461a-b088-233f5a2b5061'}.GetEnumerator()) {
            $element=$project.CreateElement($pair.Key); $element.InnerText=$pair.Value; [void]$item.AppendChild($element)
        }
        [void]$fileClass.AppendChild($item)
    }
    foreach($key in @('COptionI-0','AsmOptionI-0')) {
        $node=$project.SelectSingleNode('//*[local-name()="'+$key+'"]')
        $paths=@($node.InnerText -split '\r?\n' | Where-Object { $_ })
        foreach($dir in @(Get-ChildItem -LiteralPath $generated -Recurse -File -Filter '*.h' |
            ForEach-Object { $_.DirectoryName } | Sort-Object -Unique)) {
            $include='..\f24_board\'+$dir.Substring($board.Length+1)
            if ($include -notin $paths) { $paths+=$include }
        }
        $node.InnerText=($paths -join "`r`n")+"`r`n"
    }
    $project.SelectSingleNode('//GenerateFileFolder').InnerText=$board
    $project.SelectSingleNode('//SCFGFile').InnerText='f24_board.scfg'
    # Relocate the original project-wizard provenance, not build registrations.
    $wizardFiles=@((Join-Path $folder 'main.c'), (Join-Path $projection 'cstart.asm'),
        (Join-Path $board 'hdwinit.asm'),
        (Join-Path $generated 'r_bsp\board\generic_rl78_f24\stkinit.asm'),
        (Join-Path $board 'f24opt.asm'), (Join-Path $board 'iodefine.h'))
    for($index=0;$index -lt $wizardFiles.Count;$index++) {
        $node=$project.SelectSingleNode('//ProjectGeneratr-GeneratedFile'+$index)
        if ($node) { $node.InnerText=$wizardFiles[$index] }
    }
    foreach($node in $project.SelectNodes('//*[contains(local-name(),"DownloadItem0-FileName")]')) {
        if ($node.InnerText -eq 'DefaultBuild\RL78_F24_CAN_host_upgrade.abs') {
            $node.InnerText='DefaultBuild\'+$name+'.abs'
        }
    }
    $project.Save($projectPath)
    if ($mode -eq 'app') {
        & (Join-Path $PSScriptRoot 'prepare_f24_app_irq.ps1') -AppProject $folder
    } else {
        $boot="; Generated hardware IRQ dispatch; PSW is restored by the selected ISR RETI.`r`n.PUBLIC _F24Bridge_Jump`r`n.EXTERN _R_Config_UART0_Stop`r`n.EXTERN _R_Config_TAU0_1_Stop`r`n"
        foreach($h in @($bootHandlers.Values | Sort-Object -Unique)) { $boot+=".EXTERN _$h`r`n" }
        for($v=4;$v -le 0x7C;$v+=2) {
            $label='bridge_irq_'+$v.ToString('X2'); $appLabel='app_irq_'+$v.ToString('X2')
            $target=if($bootHandlers.ContainsKey($v)){'_'+$bootHandlers[$v]}else{'bridge_default_isr'}
            $boot+="$label .VECTOR 0x$($v.ToString('X2'))`r`n.SECTION .text, TEXT`r`n${label}:`r`n"
            $boot+="    CMP !0x$dispatch,#0x$appOwner`r`n    BZ `$${appLabel}`r`n    BR !!$target`r`n${appLabel}:`r`n    BR !!0x$(($start+$v*2).ToString('X5'))`r`n"
        }
        $boot+="bridge_default_isr:`r`n    RETI`r`n_F24Bridge_Jump:`r`n    DI`r`n    CALL !!_R_Config_UART0_Stop`r`n    CALL !!_R_Config_TAU0_1_Stop`r`n    MOV !0x$dispatch,#0x$appOwner`r`n    BR !!0x$($start.ToString('X5'))`r`n"
        WriteGenerated (Join-Path $folder 'bridge_vectors.asm') $boot
    }
}
Write-Host "Prepared F24 $Kind from shared board sources and the two owned projects; no standalone project."
