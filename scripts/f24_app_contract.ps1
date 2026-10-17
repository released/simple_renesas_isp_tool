# Shared app-local IRQ/identity contract. This module never writes boot files.
. (Join-Path $PSScriptRoot 'device_app_config.ps1')

function Read-F24MetadataConfig {
    $repo=Split-Path -Parent $PSScriptRoot
    $path=Join-Path $repo 'firmware\f24_bridge_common\image_metadata_config.h'
    $body=Get-AppConfigBody $path 'IMAGE_METADATA_CONFIG_H'
    $values=@{}
    foreach($line in $body.Split("`n")) {
        if(!$line.Trim()) { continue }
        $definition=[regex]::Match($line,'^\s*#define\s+F24_BRIDGE_([A-Z_]+)\s+(.+?)\s*$')
        if(!$definition.Success -or $values.ContainsKey($definition.Groups[1].Value)) {
            throw 'Invalid/duplicate host image_metadata_config.h definition.'
        }
        $name=$definition.Groups[1].Value; $literal=$definition.Groups[2].Value
        if($name -in @('FAMILY','PART')) {
            if($literal -notmatch '^"([\x20-\x21\x23-\x5B\x5D-\x7E]+)"$') { throw 'Use printable ASCII identity without escapes.' }
            $values[$name]=$Matches[1]
        } else { $values[$name]=ConvertFrom-AppUnsignedLiteral $literal }
    }
    $expected='APP_LIMIT,APP_START,BOARD_ID,BOOT_VERSION,BUILD,ERASE_SIZE,FAMILY,FLASH_SIZE,IRQ_ABI,MINIMUM_SIZE,PART,UART_BAUD'
    if((@($values.Keys | Sort-Object) -join ',') -cne $expected) { throw 'Incomplete host image metadata contract.' }
    if(!$values.ERASE_SIZE -or $values.ERASE_SIZE%4 -or !$values.APP_START -or
        $values.APP_START%$values.ERASE_SIZE -or $values.APP_LIMIT%$values.ERASE_SIZE -or
        $values.APP_LIMIT -le $values.APP_START -or $values.APP_LIMIT -gt $values.FLASH_SIZE -or
        $values.MINIMUM_SIZE -lt 516 -or $values.MINIMUM_SIZE%4 -or
        $values.MINIMUM_SIZE -gt ($values.APP_LIMIT-$values.APP_START) -or
        !$values.BOARD_ID -or $values.BOARD_ID -ge 65535 -or
        $values.BOOT_VERSION -gt 0xFFFFFF -or $values.BUILD -eq [uint32]::MaxValue -or
        !$values.IRQ_ABI -or $values.IRQ_ABI -eq [uint32]::MaxValue -or
        $values.UART_BAUD -ne 115200 -or $values.FAMILY.Length -gt 12 -or $values.PART.Length -gt 16) {
        throw 'Invalid host geometry/identity or fixed Misc115200 contract.'
    }
    return $values
}

function Read-F24HandoffConfig {
    $repo=Split-Path -Parent $PSScriptRoot
    $body=Get-AppConfigBody (Join-Path $repo 'firmware\f24_bridge_common\boot_app_handoff.h') 'BOOT_APP_HANDOFF_H'
    $values=@{}
    foreach($line in $body.Split("`n")) {
        if(!$line.Trim()) { continue }
        $definition=[regex]::Match($line,'^\s*#define\s+F24_BRIDGE_([A-Z_]+)\s+(.+?)\s*$')
        if(!$definition.Success -or $values.ContainsKey($definition.Groups[1].Value)) { throw 'Invalid/duplicate host RAM handoff definition.' }
        $values[$definition.Groups[1].Value]=ConvertFrom-AppUnsignedLiteral $definition.Groups[2].Value
    }
    if((@($values.Keys | Sort-Object) -join ',') -cne 'APP_OWNER,BOOT_COOKIE,BOOT_OWNER,COOKIE_ADDRESS,DISPATCH_ADDRESS' -or
        $values.COOKIE_ADDRESS -lt 0xF9F00 -or $values.COOKIE_ADDRESS -gt 0xFFFFC -or $values.COOKIE_ADDRESS%4 -or
        $values.DISPATCH_ADDRESS -lt 0xF9F00 -or $values.DISPATCH_ADDRESS -gt 0xFFFFC -or $values.DISPATCH_ADDRESS%4 -or
        ($values.DISPATCH_ADDRESS -ge $values.COOKIE_ADDRESS -and $values.DISPATCH_ADDRESS -le $values.COOKIE_ADDRESS+3) -or
        !$values.BOOT_COOKIE -or $values.BOOT_COOKIE -eq [uint32]::MaxValue -or
        $values.APP_OWNER -gt 255 -or $values.BOOT_OWNER -gt 255 -or $values.APP_OWNER -eq $values.BOOT_OWNER) {
        throw 'Invalid host RAM cookie/dispatch contract.'
    }
    return $values
}

function Read-F24AppImageConfig([string]$Project, [uint32]$ExpectedBoardId) {
    $body=Get-AppConfigBody (Join-Path $Project 'app_image_config.h') 'APP_IMAGE_CONFIG_H'
    $values=@{}
    foreach($line in $body.Split("`n")) {
        if(!$line.Trim()) { continue }
        $definition=[regex]::Match($line,'^\s*#define\s+(APP_IMAGE_[A-Z_]+)\s+(.+?)\s*$')
        if(!$definition.Success -or $values.ContainsKey($definition.Groups[1].Value)) { throw 'Invalid/duplicate Host App image definition.' }
        $values[$definition.Groups[1].Value]=ConvertFrom-AppUnsignedLiteral $definition.Groups[2].Value
    }
    if((@($values.Keys | Sort-Object) -join ',') -cne 'APP_IMAGE_BUILD,APP_IMAGE_ROLE,APP_IMAGE_TARGET_ID,APP_IMAGE_UPDATE_TRANSPORTS,APP_IMAGE_VERSION') {
        throw 'Host app_image_config.h requires version/build/target ID/role/update transports.'
    }
    if(!$values.APP_IMAGE_VERSION -or $values.APP_IMAGE_VERSION -gt 0xFFFFFF -or $values.APP_IMAGE_BUILD -eq [uint32]::MaxValue) {
        throw 'App version must be nonzero0x00MMmmpp and build below0xFFFFFFFF.'
    }
    if($values.APP_IMAGE_TARGET_ID -ne $ExpectedBoardId -or $values.APP_IMAGE_ROLE -ne 2 -or $values.APP_IMAGE_UPDATE_TRANSPORTS -ne 4) {
        throw 'Host App target/role/transport must match HOST_APP/UART and the Boot board identity.'
    }
    return $values
}

function Get-F24AppContract([string]$AppProject) {
    $root = (Resolve-Path -LiteralPath $AppProject).Path
    $projects = @(Get-ChildItem -LiteralPath $root -Filter '*.mtpj' -File)
    if ($projects.Count -ne 1) { throw 'AppProject must contain exactly one MTPJ.' }
    [xml]$project = [IO.File]::ReadAllText($projects[0].FullName)
    $devices = @($project.SelectNodes('//DeviceName') | ForEach-Object { $_.InnerText })
    if (!$devices.Count -or @($devices | Where-Object { $_ -ne 'R7F124FLJ3xFB' }).Count) {
        throw 'This fixed IRQ/image contract supports only the R7F124FLJ3xFB project device.'
    }
    $defines = $project.SelectSingleNode('//*[local-name()="COptionD-0"]')
    if (!$defines -or $defines.InnerText -notmatch '(?m)^\s*BRIDGE_SPLIT_APP(?:=1)?\s*$') {
        throw 'Enable BRIDGE_SPLIT_APP in the split app project definitions.'
    }
    $entries = Read-AppIrqConfig $root
    $repo = Split-Path -Parent $PSScriptRoot
    $header = [IO.File]::ReadAllText((Join-Path $repo 'firmware\f24_board\src\smc_gen\r_bsp\mcu\rl78_f24\register_access\ccrl\iodefine.h'))
    $valid = @{}
    foreach ($m in [regex]::Matches($header,'(?m)^#define\s+INT\w+\s+(0x[0-9A-Fa-f]+)\s*$')) {
        $v = [Convert]::ToInt32($m.Groups[1].Value.Substring(2),16)
        if ($v -ge 4 -and $v -le 0x7C -and !($v -band 1)) { $valid[$v] = $true }
    }
    $handlers = @{}
    foreach ($v in $entries.Keys) {
        if (!$valid.ContainsKey($v)) { throw "Reserved/non-normal F24 vector: $v" }
        $handlers[$v] = $entries[$v]
    }
    $sources = @()
    $tableRegistered = 0
    foreach ($file in $project.SelectNodes('//Instance[Type="File"]')) {
        $path = [string]$file.RelativePath
        if (!$path) { continue }
        if (![IO.Path]::IsPathRooted($path)) { $path = Join-Path $root $path }
        $path = [IO.Path]::GetFullPath($path)
        if ($path -eq (Join-Path $root 'bridge_vectors.asm')) { $tableRegistered++ }
        if ([IO.Path]::GetExtension($path) -ne '.c') { continue }
        $text = [IO.File]::ReadAllText($path)
        $text = [regex]::Replace($text, '(?s)/\*.*?\*/|(?m)//[^\r\n]*', '')
        if ($text -match '(?m)^\s*#pragma\s+interrupt\s+\w+[^\r\n]*\bvect\s*=') {
            throw "App source owns a physical vector; use a no-vect projection: $path"
        }
        $sources += $text
    }
    if ($tableRegistered -ne 1) { throw 'Register app-local bridge_vectors.asm exactly once in the MTPJ.' }
    foreach ($symbol in @($handlers.Values | Sort-Object -Unique)) {
        $escaped = [regex]::Escape($symbol)
        $pragma = '(?m)^\s*#pragma\s+interrupt\s+'+$escaped+'\s*\r?$'
        $body = '(?m)^\s*void\s+(?:__near\s+)?'+$escaped+'\s*\(\s*void\s*\)\s*\{'
        $matches = @($sources | Where-Object { $_ -match $pragma -and $_ -match $body })
        if ($matches.Count -ne 1) { throw "IRQ $symbol needs one registered global void ISR with a no-vect interrupt pragma." }
    }
    $metadata=Read-F24MetadataConfig
    if($metadata.PART -ne $devices[0]) { throw 'Host metadata part must match the selected CS+ project device.' }
    $identity=Read-F24AppImageConfig $root $metadata.BOARD_ID
    $version=$identity.APP_IMAGE_VERSION; $build=$identity.APP_IMAGE_BUILD
    return [pscustomobject]@{ Root=$root; Project=$projects[0].FullName; Name=$projects[0].BaseName;
        Handlers=$handlers; Version=$version; Build=$build }
}

function Get-F24AppVectorText($Contract) {
    $text = "; Generated from this app's app_irq_handlers.h by prepare_f24_app_irq.ps1.`r`n"
    $text += ".PUBLIC _bridge_default_isr`r`n.EXTERN _start`r`n.SECTION .app_vectors, TEXT`r`n"
    foreach ($symbol in @($Contract.Handlers.Values | Sort-Object -Unique)) { $text += ".EXTERN _$symbol`r`n" }
    for ($v=0; $v -le 0x7E; $v+=2) {
        $target = if (!$v) {'_start'} elseif ($v -eq 0x7E) {'_bridge_default_brk'} elseif ($Contract.Handlers.ContainsKey($v)) {'_'+$Contract.Handlers[$v]} else {'_bridge_default_isr'}
        $text += "    BR !!$target`r`n"
    }
    $text += ".SECTION .text, TEXT`r`n_bridge_default_isr:`r`n    RETI`r`n_bridge_default_brk:`r`n    RETB`r`n"
    return $text
}

function Assert-F24AppIrqImage($Contract, [string]$Map, [byte[]]$Image, [uint32]$Start) {
    function Symbol([string]$name) {
        $m = [regex]::Match($Map, '(?m)^\s*'+[regex]::Escape($name)+'\s*\r?\n\s*([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]+)\s+(?:func|none)')
        if (!$m.Success) { throw "Missing linked IRQ/reset symbol: $name" }
        return @([Convert]::ToUInt32($m.Groups[1].Value,16),[Convert]::ToUInt32($m.Groups[2].Value,16))
    }
    for ($v=0; $v -le 0x7E; $v+=2) {
        $name = if (!$v) {'_start'} elseif ($v -eq 0x7E) {'_bridge_default_brk'} elseif ($Contract.Handlers.ContainsKey($v)) {'_'+$Contract.Handlers[$v]} else {'_bridge_default_isr'}
        $symbol = Symbol $name
        $i = $v*2
        $actual = [uint32]$Image[$i+1] -bor ([uint32]$Image[$i+2]-shl8) -bor ([uint32]$Image[$i+3]-shl16)
        if ($Image[$i] -ne 0xEC -or $actual -ne $symbol[0]) { throw "App IRQ slot0x$($i.ToString('X')) differs from local mapping $name" }
        if (!$v) { continue }
        $last = if ($name -eq '_bridge_default_isr' -or $name -eq '_bridge_default_brk') { $actual-$Start } else { $actual-$Start+$symbol[1]-2 }
        $return = if ($v -eq 0x7E) {0xEC} else {0xFC}
        if ($last -lt 0 -or $last+1 -ge $Image.Length-4 -or $Image[$last] -ne 0x61 -or $Image[$last+1] -ne $return) {
            throw "IRQ $name lacks the expected real RETI/RETB epilogue. Inspect compiler output."
        }
    }
    Write-Host 'PASS: all64 app slots match local mapping and real ISR RETI/BRK RETB.'
}
