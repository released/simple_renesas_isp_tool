param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo ('tmp\internal\tests\serial_scfg_guards_'+[guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($out)|Out-Null
$count=0
function Save-Xml($Xml,[string]$Path){
    $settings=New-Object Xml.XmlWriterSettings;$settings.Encoding=New-Object Text.UTF8Encoding($false)
    $w=[Xml.XmlWriter]::Create($Path,$settings);try{$Xml.Save($w)}finally{$w.Dispose()}
}
foreach($mcu in @('F24','G16','G22')){foreach($bus in @('UART','I2C')){
    $name='RL78_'+$mcu+'_ISP_Boot_loader_'+$bus;$source=Join-Path $DeviceRoot $name;$folder=Join-Path $out $name
    [IO.Directory]::CreateDirectory($folder)|Out-Null
    foreach($file in @(($name+'.scfg'),($name+'.mtpj'),'validate_scfg.ps1','isp_transport_config.h','sc_peripheral_config.generated.h')){
        Copy-Item -LiteralPath (Join-Path $source $file) -Destination (Join-Path $folder $file)
    }
    [IO.Directory]::CreateDirectory((Join-Path $folder 'src'))|Out-Null
    Copy-Item -LiteralPath (Join-Path $source 'src\smc_gen') -Destination (Join-Path $folder 'src') -Recurse
    $scfg=Join-Path $folder ($name+'.scfg');$mtpj=Join-Path $folder ($name+'.mtpj');$header=Join-Path $folder 'sc_peripheral_config.generated.h'
    $scText=[IO.File]::ReadAllText($scfg);$projectText=[IO.File]::ReadAllText($mtpj);$headerText=[IO.File]::ReadAllText($header)
    & (Join-Path $folder 'validate_scfg.ps1') -Project $folder -CheckOnly
    $cases=if($bus -eq 'UART'){@('MCU','pin','clock','clock-source','timer','baud','parity','stale-uart','projection','association')}else{@('address','stale-i2c')}
    foreach($case in $cases){
        [IO.File]::WriteAllText($scfg,$scText,(New-Object Text.UTF8Encoding($false)))
        [IO.File]::WriteAllText($mtpj,$projectText,(New-Object Text.UTF8Encoding($false)))
        [IO.File]::WriteAllText($header,$headerText,(New-Object Text.UTF8Encoding($false)))
        [xml]$sc=$scText;$changedFile='';$before=''
        switch($case){
            MCU {$sc.smc.general.platform.id='INVALID_MCU'}
            pin {$sc.SelectSingleNode('//tool[@id="Pins"]/pinItem').SetAttribute('allocation','0')}
            clock {$sc.SelectSingleNode('//tool[@id="Clock"]/option[@id="hsocofrequency"]').SetAttribute('selection','frq8');if($mcu -eq 'F24'){$sc.SelectSingleNode('//tool[@id="Clock"]/option[@id="hssclockenable"]').SetAttribute('selection','uncheck')}}
            clock-source {$sc.SelectSingleNode('//tool[@id="Clock"]/option[@id="fmainswitcher"]').SetAttribute('selection','EXTERNAL')}
            timer {$sc.SelectSingleNode('//configuration[@name="Config_TAU0_1"]//option[@id="IntervalValue16bits"]/item[@id="intervalValueText16bits"]').SetAttribute('input','2')}
            baud {$sc.SelectSingleNode('//configuration[@name="Config_UART1"]//option[@id="baudrateSetting"]/item[@id="input"]').SetAttribute('input','57600')}
            parity {$sc.SelectSingleNode('//configuration[@name="Config_UART1"]//option[@id="paritySetting"]').SetAttribute('selection','even')}
            address {$sc.SelectSingleNode('//configuration[@name="Config_IICA0"]//option[@id="address"]/item[@id="input"]').SetAttribute('input','32')}
            stale-uart {$changedFile=Join-Path $folder 'src\smc_gen\Config_UART1\Config_UART1.c';$before=[IO.File]::ReadAllText($changedFile);$after=[regex]::Replace($before,'(SDR(?:10|02)\s*=\s*)_[0-9A-F]+_','$1_0000_');[IO.File]::WriteAllText($changedFile,$after,(New-Object Text.UTF8Encoding($false)))}
            stale-i2c {$changedFile=Join-Path $folder 'src\smc_gen\Config_IICA0\Config_IICA0.c';$before=[IO.File]::ReadAllText($changedFile);$after=[regex]::Replace($before,'(SVA0\s*=\s*)_[0-9A-F]+_','$1_0000_');[IO.File]::WriteAllText($changedFile,$after,(New-Object Text.UTF8Encoding($false)))}
            projection {[IO.File]::WriteAllText($header,$headerText.Replace('ISP_SCFG_CPU_HZ','BROKEN_CPU'),(New-Object Text.UTF8Encoding($false)))}
            association {[xml]$p=$projectText;$p.SelectSingleNode('//SCFGFile').InnerText='wrong.scfg';Save-Xml $p $mtpj}
        }
        Save-Xml $sc $scfg
        $rejected=$false
        try{& (Join-Path $folder 'validate_scfg.ps1') -Project $folder -CheckOnly}catch{$rejected=$true;Write-Host ('PASS rejected '+$mcu+'/'+$bus+'/'+$case+': '+$_.Exception.Message)}
        if($changedFile){[IO.File]::WriteAllText($changedFile,$before,(New-Object Text.UTF8Encoding($false)))}
        if(!$rejected){throw ('Missing SC guard: '+$mcu+'/'+$bus+'/'+$case)}
        ++$count
    }
    if($bus -eq 'UART'){
        [xml]$sc=$scText
        $sc.SelectSingleNode('//configuration[@name="Config_UART1"]//option[@id="baudrateSetting"]/item[@id="input"]').SetAttribute('input','57600')
        Save-Xml $sc $scfg
        [IO.File]::WriteAllText($mtpj,$projectText,(New-Object Text.UTF8Encoding($false)))
        [IO.File]::WriteAllText($header,$headerText,(New-Object Text.UTF8Encoding($false)))
        $sentinel=Join-Path $folder 'ftable.inc';[IO.File]::WriteAllText($sentinel,'SC_FAIL_FAST_SENTINEL')
        $rejected=$false
        try{
            if($mcu -eq 'F24'){& (Join-Path $source 'tools\prepare_projects.ps1') -Kind boot -Project $folder -ArtifactsOnly}
            else{& (Join-Path $source 'generate_irq_contract.ps1') -Mode Boot -OutputProject $folder}
        }catch{
            if($_.Exception.Message -notmatch 'SC consistency:.*UART1 baud'){throw}
            $rejected=$true
        }
        if(!$rejected -or [IO.File]::ReadAllText($sentinel) -cne 'SC_FAIL_FAST_SENTINEL'){throw 'Actual pre-build did not stop before generation.'}
        Write-Host ('PASS actual '+$mcu+' pre-build hook fails before metadata/vector generation.')
    }
}}
Write-Host ('PASS '+$count+' SC negative cases. Originals not modified. '+$out)
