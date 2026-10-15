param([ValidateSet('boot','app')][string]$Kind='app',[string]$AppProject='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$layout=Get-Content (Join-Path $repo 'firmware\f24_bridge_common\f24_bridge_layout.h') -Raw
function Value([string]$name) {
    $m=[regex]::Match($layout,"#define F24_BRIDGE_$name \((0x[0-9A-Fa-f]+|[0-9]+)[UL]*\)")
    if (!$m.Success) { throw "Missing layout value $name" }
    return [Convert]::ToUInt32($m.Groups[1].Value.Replace('0x',''),$(if($m.Groups[1].Value.StartsWith('0x')){16}else{10}))
}
$start=Value 'APP_START'; $limit=Value 'APP_LIMIT'; $flash=Value 'FLASH_SIZE'
$name='RL78_F24_Bridge_'+$Kind
if ($Kind -eq 'app') {
    if (!$AppProject) { $AppProject=Join-Path $repo 'firmware\RL78_F24_Bridge_app' }
    . (Join-Path $PSScriptRoot 'f24_app_contract.ps1')
    $appContract=Get-F24AppContract $AppProject
    & (Join-Path $PSScriptRoot 'prepare_f24_app_irq.ps1') -AppProject $appContract.Root -CheckOnly
    $name=$appContract.Name
    $folder=Join-Path $appContract.Root 'DefaultBuild'
} else {
    if ($AppProject) { throw 'AppProject is only valid with Kind app.' }
    $folder=Join-Path $repo ('firmware\'+$name+'\DefaultBuild')
}
$map=Get-Content (Join-Path $folder ($name+'.map')) -Raw
$mapping=($map -split '\*\*\* Mapping List \*\*\*')[1] -split '\*\*\* Total Section Size \*\*\*'
if (!$mapping) { throw 'No linker map section evidence' }
$sections=@{}
foreach($m in [regex]::Matches($mapping[0],'(?m)^([.\w]+)\r?\n\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]+)\s')) {
    $n=$m.Groups[1].Value; $a=[Convert]::ToUInt32($m.Groups[2].Value,16)
    $end=[Convert]::ToUInt32($m.Groups[3].Value,16); $size=[Convert]::ToUInt32($m.Groups[4].Value,16)
    $sections[$n]=@($a,$end,$size)
    if (!$size) { continue }
    if ($a -ge 0xF0000) {
        if ($a -le 0xFF713 -and $end -ge 0xFF700) { throw "RAM $n overwrites handoff region" }
    } elseif ($n -notin @('.monitor2','.option_byte','.security_id','.flash_security_id','.monitor1','.vect')) {
        if ($Kind -eq 'boot' -and $end -ge $start) { throw "Boot section $n crosses app boundary" }
        if ($Kind -eq 'app' -and ($a -lt $start -or $end -ge $limit)) { throw "App section $n outside allowed region" }
    }
}
if (!$sections.ContainsKey('.stack_bss') -or $sections['.stack_bss'][2] -ne 0x1000) { throw 'Explicit stack missing' }
if ($sections.ContainsKey('.const') -and $sections['.const'][2]) { throw 'Split firmware must use far ROM constants' }
if ($Kind -eq 'boot') {
    $physical=($map -split '\*\*\* Variable Vector Table List \*\*\*')[1] -split '\*\*\*'
    $brk=[regex]::Match($physical[0],'(?m)^7e[^\S\r\n]*([^\r\n]*)\r?$')
    if (!$brk.Success -or $brk.Groups[1].Value.Trim()) { throw 'Boot must leave BRK007E unbound for debugger/device file' }
    foreach($s in @('RFD_CMN_fR','RFD_CF_fR','BRAM_fR','RFD_DATA_nR')) {
        if (!$sections.ContainsKey($s) -or !$sections[$s][2] -or $sections[$s][0] -lt 0xF9F00 -or $sections[$s][1] -ge 0xFF700) { throw "RFD RAM relocation invalid: $s" }
        $rom=$s.Substring(0,$s.Length-1)
        if ($sections[$rom][2] -ne $sections[$s][2]) { throw "ROM/RAM copy length mismatch: $s" }
    }
    $symbols=($map -split '\*\*\* Symbol List \*\*\*')[1] -split '\*\*\* Delete Symbols \*\*\*'
    $ramFunctions=[regex]::Matches($symbols[0],'(?m)^  (_(?:R_RFD_\w+|r_rfd_wait_count|execute@\d+|wait_end@\d+|fail_reset@\d+))\r?\n\s+([0-9a-fA-F]{8})\s+[^\r\n]*func')
    if ($ramFunctions.Count -lt 10) { throw 'Incomplete RAM function symbol audit' }
    foreach($m in $ramFunctions) {
        $a=[Convert]::ToUInt32($m.Groups[2].Value,16)
        if ($a -lt 0xF9F00 -or $a -ge 0xFF700) { throw "Flash-mode function not in RAM: $($m.Groups[1].Value)" }
    }
    $references=($map -split '\*\*\* Cross Reference List \*\*\*')[1]
    $xref=[regex]::Matches($references,'(?m)^\s+(\S+)\r?\n\s+([0-9a-fA-F]{8})([^\r\n]*(?:\r?\n {35,}[^\r\n]+)*)')
    if ($xref.Count -lt 10) { throw 'Incomplete flash-mode reference audit' }
    foreach($m in $xref) {
        $target=[Convert]::ToUInt32($m.Groups[2].Value,16)
        if ($target -lt 0x40000 -and $m.Groups[3].Value -match '\([0-9a-fA-F]{8}:(?:BRAM_fR|RFD_CMN_fR|RFD_CF_fR)\)') {
            throw "RAM flash-mode code references code flash: $($m.Groups[1].Value)"
        }
    }
    Write-Host 'PASS: boot ROM <0x5000, RFD/driver RAM copies and protected handoff/stack geometry.'
    return
}
if (!$sections.ContainsKey('.app_vectors') -or $sections['.app_vectors'][0] -ne $start -or $sections['.app_vectors'][2] -ne 256) { throw 'App IRQ table is not exactly64 x4 bytes at app start' }
$vectorList=($map -split '\*\*\* Variable Vector Table List \*\*\*')[1] -split '\*\*\*'
if (!$vectorList) { throw 'Missing physical vector symbol audit' }
foreach($line in ($vectorList[0] -split '\r?\n')) {
    if ($line -match '^([0-9a-fA-F]{2})\s+(\S+)') {
        if ($matches[1] -ne '02' -or $matches[2] -ne 'ffff') { throw "App owns a physical vector: $line" }
    }
}
# The device file emits an unbound .vect placeholder. Never distribute the
# raw app MOT/ABS for initial flashing; the packaged _flash.mot excludes it.
$records=New-Object 'System.Collections.Generic.SortedDictionary[uint32,byte]'
foreach($line in Get-Content (Join-Path $folder ($name+'.mot'))) {
    if ($line -notmatch '^S([123])([0-9A-Fa-f]+)$') { continue }
    $addressBytes=[int]$matches[1]+1; $hex=$matches[2]
    $bytes=New-Object byte[] ($hex.Length/2)
    $sum=0
    for($i=0;$i -lt $bytes.Length;$i++) { $bytes[$i]=[Convert]::ToByte($hex.Substring($i*2,2),16); $sum+=$bytes[$i] }
    if ($bytes[0] -ne $bytes.Length-1 -or ($sum-band255) -ne 255) { throw 'Invalid MOT record checksum/length' }
    [uint32]$address=0
    for($i=1;$i -le $addressBytes;$i++) { $address=($address-shl8)-bor$bytes[$i] }
    $count=$bytes.Length-$addressBytes-2
    for($i=0;$i -lt $count;$i++) {
        $a=$address+[uint32]$i
        if ($a -ge $start -and $a -lt $limit) {
            $b=$bytes[$i+$addressBytes+1]
            if ($records.ContainsKey($a) -and $records[$a] -ne $b) { throw 'Conflicting MOT records' }
            $records[$a]=$b
        } elseif ($a -ge $start -and $a -lt $flash -and $a -lt 0x3FF00) { throw 'MOT app writes reserved flash' }
    }
}
if (!$records.Count) { throw 'Empty app code' }
$last=($records.Keys | Select-Object -Last 1)
$size=[uint32](([Math]::Ceiling(($last-$start+1)/4.0)*4)+4)
if ($size -lt (Value 'MINIMUM_SIZE')) { $size=Value 'MINIMUM_SIZE' }
if ($size -gt $limit-$start) { throw 'App plus CRC exceeds partition capacity' }
$image=New-Object byte[] $size
for($i=0;$i -lt $image.Length;$i++) { $image[$i]=255 }
foreach($pair in $records.GetEnumerator()) { $image[$pair.Key-$start]=$pair.Value }
for($i=0x100;$i -lt 0x200;$i++) { if($image[$i] -ne 255) { throw '256-byte metadata region overlaps linked code/data' } }
# Four-byte BR !! entries: the reset target must point into actual app code.
for($i=0;$i -lt 256;$i+=4) {
    if ($image[$i] -ne 0xEC) { throw "IRQ table entry $i is not a four-byte BR !!" }
    $target=[uint32]$image[$i+1] -bor ([uint32]$image[$i+2]-shl8) -bor ([uint32]$image[$i+3]-shl16)
    if ($target -lt $start+0x200 -or $target -ge $start+$size-4) { throw "IRQ target outside code: $target" }
}
function Put32([int]$offset,[uint32]$number) { [BitConverter]::GetBytes($number).CopyTo($image,0x100+$offset) }
Assert-F24AppIrqImage $appContract $map $image $start
for($i=0x100;$i -lt 0x180;$i++) { $image[$i]=0 }
Put32 0 0x31525542; $image[0x104]=3; $image[0x105]=128; $image[0x106]=2; $image[0x107]=1
[BitConverter]::GetBytes([uint16](Value 'BOARD_ID')).CopyTo($image,0x108); $image[0x10A]=2
Put32 12 $size; Put32 16 $appContract.Version; Put32 20 $appContract.Build; Put32 24 $start
Put32 28 ($limit-$start); Put32 32 $flash; Put32 36 $start; Put32 40 (Value 'ERASE_SIZE')
Put32 44 (Value 'MINIMUM_SIZE'); Put32 48 (Value 'UART_BAUD'); Put32 80 (Value 'BOOT_VERSION')
Put32 96 0x31445055; $image[0x164]=1; $image[0x165]=32; $image[0x166]=2; $image[0x167]=4
Put32 104 0x31563246; Put32 108 (Value 'BOOT_VERSION')
[Text.Encoding]::ASCII.GetBytes('RL78/F24').CopyTo($image,0x134)
[Text.Encoding]::ASCII.GetBytes('R7F124FLJ3xFB').CopyTo($image,0x140)
if (-not ('F24BridgeCrc' -as [type])) {
    Add-Type 'public static class F24BridgeCrc { public static uint Calculate(byte[] b,int n) {uint c=0xffffffff;for(int i=0;i<n;i++){c^=b[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)!=0?0xedb88320u:0u);}return c^0xffffffff;} }'
}
$crc=[F24BridgeCrc]::Calculate($image,$image.Length-4)
[BitConverter]::GetBytes($crc).CopyTo($image,$image.Length-4)
$output=Join-Path $folder ($name+'_bridge_update.bin')
[IO.File]::WriteAllBytes($output,$image)
$mot=New-Object 'System.Collections.Generic.List[string]'
for($i=0;$i -lt $image.Length;$i+=32) {
    $n=[Math]::Min(32,$image.Length-$i); $address=$start+$i
    $line='S3'+('{0:X2}{1:X8}' -f ($n+5),$address); $sum=$n+5
    for($shift=0;$shift -lt 32;$shift+=8) { $sum+=($address-shr$shift)-band255 }
    for($j=0;$j -lt $n;$j++) { $line+=$image[$i+$j].ToString('X2'); $sum+=$image[$i+$j] }
    $line+=((255-($sum-band255))-band255).ToString('X2'); $mot.Add($line)
}
$mot.Add('S705'+$start.ToString('X8')+((255-(5+($start-band255)+(($start-shr8)-band255)+(($start-shr16)-band255)+(($start-shr24)-band255)))-band255).ToString('X2'))
[IO.File]::WriteAllLines((Join-Path $folder ($name+'_flash.mot')),$mot,[Text.Encoding]::ASCII)
Write-Host ('PASS: dynamic bridge app {0} bytes, CRC32 0x{1:X8}, start0x{2:X}, capacity{3}. Source MOT unchanged.' -f $size,$crc,$start,($limit-$start))
