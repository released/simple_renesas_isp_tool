param([Parameter(Mandatory=$true)][string]$ProjectRoot,
      [Parameter(Mandatory=$true)][string]$MapFile)
$ErrorActionPreference='Stop'
$project=(Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\')
$map=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($MapFile)){$MapFile}else{Join-Path $project $MapFile}))
if(!$map.StartsWith($project+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'App handoff map must belong to the selected project.'
}
$header=[IO.File]::ReadAllText((Join-Path $project 'boot_app_handoff.h'))
$header=[regex]::Replace($header,'(?s)/\*.*?\*/|//[^\r\n]*','')
function Read-HandoffMacro([string]$Name) {
    $matches=[regex]::Matches($header,'(?m)^#define\s+'+[regex]::Escape($Name)+'\s+\((0x[0-9a-fA-F]+|[0-9]+)[UL]*\)\s*$')
    if($matches.Count -ne 1){throw ('Missing or duplicate handoff macro: '+$Name)}
    $text=$matches[0].Groups[1].Value
    return [Convert]::ToInt64($text.Replace('0x',''),$(if($text.StartsWith('0x')){16}else{10}))
}
$shared=Read-HandoffMacro 'BOOT_APP_SHARED_RAM_ADDRESS'
$sharedSize=Read-HandoffMacro 'BOOT_APP_SHARED_RAM_SIZE'
if($shared -ne 0xFF710 -or $sharedSize -ne 16){throw 'Unexpected F13 handoff RAM contract.'}
$mapText=[IO.File]::ReadAllText($map)
function Read-MapSymbol([string]$Name) {
    $matches=[regex]::Matches($mapText,'(?m)^\s*'+[regex]::Escape($Name)+'\s*\r?\n\s*([0-9a-fA-F]{8})\b')
    $values=@($matches|ForEach-Object {[Convert]::ToInt64($_.Groups[1].Value,16)}|Select-Object -Unique)
    if($values.Count -ne 1){throw ('Missing or ambiguous linked RAM symbol: '+$Name)}
    return [long]$values[0]
}
$reservedStart=Read-MapSymbol '__s.boot_info'
$reservedEnd=Read-MapSymbol '__e.boot_info'
if($reservedStart -ne $shared -or $reservedEnd -ne ($shared+$sharedSize)) {
    throw 'Linked no-init handoff reservation must cover exactly FF710..FF71F.'
}
$stackLow=Read-MapSymbol '__STACK_ADDR_END'
$stackHigh=Read-MapSymbol '__STACK_ADDR_START'
if($stackHigh -le $stackLow -or ($stackHigh-$stackLow) -lt 512) {
    throw 'Linked App stack must retain at least 512 bytes.'
}
if($stackLow -lt ($shared+$sharedSize) -and $stackHigh -gt $shared) {
    throw 'App stack/stack initialization overlaps Boot/App handoff RAM.'
}
foreach($section in @('bss','sbss','dataR','sdataR')) {
    $first=Read-MapSymbol ('__s.'+$section)
    $last=Read-MapSymbol ('__e.'+$section)
    if($last -lt $first){throw ('Invalid linked RAM extent: '+$section)}
    if($last -gt $first -and $first -lt ($shared+$sharedSize) -and $last -gt $shared) {
        throw ('App startup RAM initialization overlaps handoff: '+$section)
    }
}
if($mapText -match '(?m)^\.ram_init_table\s*\r?\n\s*[0-9a-fA-F]{8}\s+[0-9a-fA-F]{8}\s+0*[1-9a-fA-F][0-9a-fA-F]*\s') {
    throw 'RAM init tables require a separately reviewed no-init handoff projection.'
}
$startup=[regex]::Replace([IO.File]::ReadAllText((Join-Path $project 'cstart.asm')),'(?m);[^\r\n]*','')
if($startup -notmatch 'MOVW\s+SP,\s*#LOWW\(__STACK_ADDR_START\)' -or
   $startup -notmatch 'MOVW\s+AX,\s*#LOWW\(__STACK_ADDR_END\)' -or
   $startup -notmatch '\.SECTION\s+\.boot_info,\s*BSS\s+_isp_handoff_ram:\s+\.DS\s+0x10\b') {
    throw 'App startup does not match the reviewed automatic-stack/no-init contract.'
}
if($startup -match '(?:STARTOF|SIZEOF)\(\.boot_info\)') {
    throw 'App startup must not initialize the no-init handoff section.'
}
Write-Host ('PASS F13 App handoff: no-init0x{0:X5}..0x{1:X5}; stack0x{2:X5}..0x{3:X5} ({4} bytes).' -f
    $shared,($shared+$sharedSize-1),$stackLow,($stackHigh-1),($stackHigh-$stackLow))
