param([Parameter(Mandatory=$true)][string]$Project,[switch]$Initialize,[switch]$Generate,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
if(($Initialize -or $Generate) -and $CheckOnly){throw 'Choose initialization/generation or CheckOnly.'}
$repo=Split-Path -Parent $PSScriptRoot
$Project=(Resolve-Path -LiteralPath $Project).Path
$name=Split-Path -Leaf $Project
if($name -notmatch '^RL78_(F24|G16|G22)_(ISP_Boot_loader_(UART|I2C|Xmodem)(_app)?|(UART|Xmodem|I2C_Slave)_Transfer)$'){throw 'Only the named SC-owned serial projects are supported.'}
$mcu=$Matches[1];$bus=if($Matches[3]){$Matches[3]}elseif($Matches[5] -eq 'I2C_Slave'){'I2C'}else{$Matches[5]}
$bootName='RL78_'+$mcu+'_ISP_Boot_loader_'+$bus
$boot=Join-Path (Split-Path -Parent $Project) $bootName
$owned=Join-Path $repo 'firmware\common\rl78_serial_scfg'
$path=Join-Path $Project ($name+'.mtpj')
if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Missing existing project.'}
foreach($item in @((Get-Item -LiteralPath $Project),(Get-Item -LiteralPath $path))){if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse project/target refused.'}}
function Assert-Target([string]$Target){
    $deviceRoot=(Split-Path -Parent $Project).TrimEnd('\');$resolved=[IO.Path]::GetFullPath($Target)
    if(!$resolved.StartsWith($deviceRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'SC target escaped the named sibling workspace.'}
    while($resolved.Length -ge $deviceRoot.Length){
        if((Test-Path -LiteralPath $resolved) -and ((Get-Item -LiteralPath $resolved -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw ('Reparse SC target refused: '+$resolved)}
        $resolved=Split-Path -Parent $resolved
    }
}
foreach($target in @((Join-Path $Project ($name+'.scfg')),(Join-Path $boot 'validate_scfg.ps1'),(Join-Path $Project 'sc_peripheral_config.generated.h'),$(if($mcu -eq 'F24'){Join-Path $boot 'tools\prepare_projects.ps1'}else{Join-Path $boot 'generate_irq_contract.ps1'}))){Assert-Target $target}
$scfg=Join-Path $Project ($name+'.scfg')
if($CheckOnly){& (Join-Path $boot 'validate_scfg.ps1') -Project $Project -CheckOnly;return}
if($Initialize -or !(Test-Path -LiteralPath $scfg)){
    $profile=$mcu+'_'+$(if($bus -eq 'I2C'){'I2C'}else{'UART'})+'.scfg'
    Copy-Item -LiteralPath (Join-Path $owned $profile) -Destination $scfg
}
Copy-Item -LiteralPath (Join-Path $owned 'validate_scfg.ps1') -Destination (Join-Path $boot 'validate_scfg.ps1')
[xml]$xml=[IO.File]::ReadAllText($path)
$root=$xml.DocumentElement;$group=$root.SelectSingleNode('Class')
foreach($guid in @('67508d11-a84e-4956-9db4-04c1eefe9e7a','cb4eebec-2381-4e77-8b34-fa8cd6f33b4c','9f7af90c-526c-4f28-af29-524df208b9e0','7592d25f-7c36-4867-babf-4cd1d5058f9b')){
    foreach($node in @($root.SelectNodes('Class[@Guid="'+$guid+'"]'))){[void]$root.RemoveChild($node)}
}
$managerGuid='5f091d5c-bde7-4b49-be6a-b6f95d1227f7';$toolGuid='4d843d40-b00f-4fff-b0c0-ffd1a818b9a3'
if(!$root.SelectSingleNode('Class[@Guid="'+$managerGuid+'"]')){
    $c=$xml.CreateElement('Class');$c.SetAttribute('Guid',$managerGuid);$i=$xml.CreateElement('Instance');$i.SetAttribute('Guid',$managerGuid)
    $n=$xml.CreateElement('SmcCodePartRL78Manager');$n.InnerText=$toolGuid;[void]$i.AppendChild($n);[void]$c.AppendChild($i);[void]$root.AppendChild($c)
}
$tool=$root.SelectSingleNode('Class[@Guid="'+$toolGuid+'"]/Instance')
if(!$tool){$c=$xml.CreateElement('Class');$c.SetAttribute('Guid',$toolGuid);$tool=$xml.CreateElement('Instance');$tool.SetAttribute('Guid',$toolGuid);[void]$c.AppendChild($tool);[void]$root.AppendChild($c)}
$category=$group.SelectSingleNode('Instance[Type="Category" and Name="Smart Configurator"]')
$fileCategory=$group.SelectSingleNode('Instance/FileCategoryGuid').InnerText
if(!$fileCategory){throw 'Missing project-owned source-tree root.'}
if(!$category){
    $category=$xml.CreateElement('Instance');$category.SetAttribute('Guid',[guid]::NewGuid().ToString())
    foreach($pair in @(@('Name','Smart Configurator'),@('Type','Category'),@('ParentItem',$fileCategory))){$n=$xml.CreateElement($pair[0]);$n.InnerText=$pair[1];[void]$category.AppendChild($n)}
    [void]$group.AppendChild($category)
}
$category.SelectSingleNode('ParentItem').InnerText=$fileCategory
foreach($oldCategory in $group.SelectNodes('Instance[Type="Category" and Name="Code Generator"]')){$oldCategory.SelectSingleNode('Name').InnerText='Device runtime'}
foreach($pair in @(@('GenerateFileFolder','.'),@('SmcExePath',(Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\SmartConfigurator\RL78\eclipse\SmartConfigurator.exe')),@('CodeGeneratorFilesGuid',$category.GetAttribute('Guid')),@('SCFGFile',($name+'.scfg')))){
    $n=$tool.SelectSingleNode($pair[0]);if(!$n){$n=$xml.CreateElement($pair[0]);[void]$tool.AppendChild($n)};$n.InnerText=$pair[1]
}
$pre=$root.SelectSingleNode('.//PreBuildCommands-0')
$lines=@($pre.InnerText -split '\r?\n'|Where-Object {$_ -and $_ -notmatch 'validate_scfg.ps1'})
$pre.InnerText=($lines -join "`n")+"`n"
$include=$root.SelectSingleNode('.//COptionPreinclude-0');if(!$include){throw 'Missing compiler preinclude option.'}
$inc=@($include.InnerText -split '\r?\n'|Where-Object {$_ -and $_ -ne 'sc_peripheral_config.generated.h'})
$include.InnerText=($inc+@('sc_peripheral_config.generated.h') -join "`n")+"`n"
$defines=$root.SelectSingleNode('.//COptionD-0')
if($defines.InnerText -notmatch '(?m)^ISP_SCFG_REQUIRED\s*$'){$defines.InnerText=$defines.InnerText.TrimEnd()+"`nISP_SCFG_REQUIRED`n"}
foreach($file in @(($name+'.scfg'),'sc_peripheral_config.generated.h')){
    if(!$group.SelectSingleNode('Instance[Type="File" and RelativePath="'+$file+'"]')){
        $node=$xml.CreateElement('Instance');$node.SetAttribute('Guid',[guid]::NewGuid().ToString())
        foreach($pair in @(@('Name',$file),@('Type','File'),@('RelativePath',$file),@('TreeImageGuid','03cad1e8-2eb3-4cde-a8a3-982423631122'),@('ParentItem',$category.GetAttribute('Guid')))){
            $child=$xml.CreateElement($pair[0]);$child.InnerText=$pair[1];[void]$node.AppendChild($child)
        }
        [void]$group.AppendChild($node)
    }
}
# The runtime owns ISR bodies, queues, mailbox and startup. SC supplies verified
# register settings through the project-local generated header, not a second ISR.
foreach($node in @($group.SelectNodes('Instance[Type="File"]'))){
    $raw=[string]$node.RelativePath
    if($raw -match '^src[\\/]smc_gen[\\/]' -and !(Test-Path -LiteralPath (Join-Path $Project $raw) -PathType Leaf)){
        [void]$group.RemoveChild($node);continue
    }
    if($raw -match '^src[\\/]smc_gen[\\/]' -and [IO.Path]::GetExtension($raw) -in @('.c','.asm')){
        if($mcu -eq 'F24' -and ($raw -match '[\\/]r_bsp[\\/]' -or $raw -match '[\\/]Config_TAU0_1[\\/]Config_TAU0_1\.c$' -or $raw -match '[\\/]r_cg_tau_common\.c$')){continue}
        [void]$group.RemoveChild($node)
    }
    if($raw -match 'r_rscanfd_rl78|Config_INTC[\\/]|Config_PORT[\\/]'){
        if($mcu -eq 'F24'){[void]$group.RemoveChild($node)}
    }
}
$settings=New-Object Xml.XmlWriterSettings;$settings.Encoding=New-Object Text.UTF8Encoding($false);$settings.Indent=$false
$writer=[Xml.XmlWriter]::Create($path,$settings);try{$xml.Save($writer)}finally{$writer.Dispose()}
if($mcu -eq 'F24'){
    $prep=Join-Path $boot 'tools\prepare_projects.ps1';$text=[IO.File]::ReadAllText($prep)
    if($text -match 'SC-owned serial pre-build'){
        $text=[regex]::Replace($text,'(?m)^    # SC-owned serial pre-build[^\r\n]*\r?\n    \$scValidator=[^\r\n]*\r?\n    \$pre=[^\r\n]*\r?\n','')
    }
    if($text -notmatch 'SC peripheral input validation'){
        $anchor='    $mtpj=Get-DeviceProjectFile $Folder'
        if(!$text.Contains($anchor)){throw 'F24 project function anchor changed.'}
        $text=$text.Replace($anchor,('    # SC peripheral input validation fails before metadata/vector generation.'+"`n"+
            '    & (Join-Path $script:DeviceBoot ''validate_scfg.ps1'') -Project $Folder -CheckOnly:$CheckOnly'+"`n"+$anchor))
        [IO.File]::WriteAllText($prep,$text,(New-Object Text.UTF8Encoding($false)))
    }
    # Prepare-DeviceProject also reconstructs preincludes during an explicit build.
    $text=[IO.File]::ReadAllText($prep)
    if($text -notmatch 'ISP_SCFG_REQUIRED'){
        $text=$text.Replace('`nISP_MCU_F24`nISP_DEVICE_','`nISP_MCU_F24`nISP_SCFG_REQUIRED`nISP_DEVICE_')
        [IO.File]::WriteAllText($prep,$text,(New-Object Text.UTF8Encoding($false)))
    }
    if($text -notmatch 'sc_peripheral_config.generated.h'){
        $key="    Set-Option 'COptionPreinclude-0' ((Get-DeviceRelative (Join-Path `$script:DeviceBoot 'common\f24_device_config.h') `$Folder)+`"``n`")"
        $replacement=$key.Substring(0,$key.Length-1)+"+'sc_peripheral_config.generated.h'+`"``n`")"
        if(!$text.Contains($key)){throw 'F24 preinclude anchor changed.'}
        $text=$text.Replace($key,$replacement)
        [IO.File]::WriteAllText($prep,$text,(New-Object Text.UTF8Encoding($false)))
    }
}else{
    $generator=Join-Path $boot 'generate_irq_contract.ps1';$text=[IO.File]::ReadAllText($generator)
    if($text -notmatch 'SC peripheral input validation'){
        $anchor="`$ErrorActionPreference = 'Stop'"
        if(!$text.Contains($anchor)){throw 'IRQ generator entry anchor changed.'}
        $insert=$anchor+"`n"+'# SC peripheral input validation fails before metadata/vector generation.'+"`n"+
            '$scTarget=if($Mode -eq ''App''){$AppProject}else{$OutputProject}'+"`n"+
            '& (Join-Path $PSScriptRoot ''validate_scfg.ps1'') -Project $scTarget -CheckOnly:$CheckOnly'
        $text=$text.Replace($anchor,$insert)
        [IO.File]::WriteAllText($generator,$text,(New-Object Text.UTF8Encoding($false)))
    }
}
if($Generate){
    $sc=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\SmartConfigurator\RL78\eclipse\SmartConfiguratorc.exe'
    if(!(Test-Path -LiteralPath $sc)){throw 'Installed RL78 Smart Configurator is required; no installation is performed.'}
    $logRoot=Join-Path $Project 'tmp\build';Assert-Target $logRoot
    [IO.Directory]::CreateDirectory($logRoot)|Out-Null
    $log=Join-Path $logRoot 'sc_generation.log'
    $args='-nosplash -application com.renesas.smc.headlessGeneration -scfg "'+$scfg+'" -data "'+(Join-Path $logRoot 'sc_workspace')+'"'
    $p=Start-Process -FilePath $sc -ArgumentList $args -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr')
    if($p.ExitCode -ne 0){throw ('SC generation failed: '+$log)}
    & (Join-Path $boot 'validate_scfg.ps1') -Project $Project
    # Generation can remove obsolete component headers. Refresh only their
    # project registrations; no generated file or user SC setting is deleted.
    & $PSCommandPath -Project $Project
}
Write-Host ('Prepared SC association/profile and validation hook: '+$name+'. Generate SC before Rebuild.')
