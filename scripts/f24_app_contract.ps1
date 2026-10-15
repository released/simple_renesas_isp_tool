# Shared app-local IRQ/identity contract. This module never writes boot files.
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
    $mapPath = Join-Path $root 'app_irq_handlers.json'
    $raw = [IO.File]::ReadAllText($mapPath)
    if (!$raw.Trim().StartsWith('{') -or !$raw.Trim().EndsWith('}')) { throw 'IRQ mapping must be a JSON object.' }
    $entries = $raw | ConvertFrom-Json
    $names = @{}
    foreach ($m in [regex]::Matches($raw, '"(0x[0-9A-Fa-f]+)"\s*:')) {
        $key = $m.Groups[1].Value.ToUpperInvariant()
        if ($names.ContainsKey($key)) { throw "Duplicate IRQ vector key: $key" }
        $names[$key] = $true
    }
    $repo = Split-Path -Parent $PSScriptRoot
    $header = [IO.File]::ReadAllText((Join-Path $repo 'firmware\f24_board\src\smc_gen\r_bsp\mcu\rl78_f24\register_access\ccrl\iodefine.h'))
    $valid = @{}
    foreach ($m in [regex]::Matches($header,'(?m)^#define\s+INT\w+\s+(0x[0-9A-Fa-f]+)\s*$')) {
        $v = [Convert]::ToInt32($m.Groups[1].Value.Substring(2),16)
        if ($v -ge 4 -and $v -le 0x7C -and !($v -band 1)) { $valid[$v] = $true }
    }
    $handlers = @{}
    foreach ($p in $entries.PSObject.Properties) {
        if ($p.Name -notmatch '^0x[0-9A-Fa-f]{2}$') { throw "Invalid vector key: $($p.Name)" }
        $v = [Convert]::ToInt32($p.Name.Substring(2),16)
        if (!$valid.ContainsKey($v)) { throw "Reserved/non-normal F24 vector: $($p.Name)" }
        if ($p.Value -isnot [string] -or $p.Value -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') { throw "Invalid IRQ symbol for $($p.Name)" }
        $handlers[$v] = [string]$p.Value
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
    $identity = [IO.File]::ReadAllText((Join-Path $root 'app_image.json')) | ConvertFrom-Json
    if ($identity.version -isnot [string] -or $identity.version -notmatch '^0x[0-9A-Fa-f]{8}$') {
        throw 'app_image.json version must be a packed 0x00MMmmpp hexadecimal string.'
    }
    $version = [Convert]::ToUInt32($identity.version.Substring(2),16)
    if (!$version -or ($version -band 0xFF000000)) { throw 'Invalid app version.' }
    if ([string]$identity.build -notmatch '^\d+$') { throw 'App build must be an unsigned integer.' }
    $build = [Convert]::ToUInt32($identity.build)
    if ($build -eq [uint32]::MaxValue) { throw 'App build must not be erasedFFFFFFFF.' }
    return [pscustomobject]@{ Root=$root; Project=$projects[0].FullName; Name=$projects[0].BaseName;
        Handlers=$handlers; Version=$version; Build=$build }
}

function Get-F24AppVectorText($Contract) {
    $text = "; Generated from this app's app_irq_handlers.json by prepare_f24_app_irq.ps1.`r`n"
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
