param(
    [string]$BootProject,
    [string]$AppProject
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$BootProject = Get-CompanionPath -Kind F13Boot -ExplicitPath $BootProject
$AppProject = Get-CompanionPath -Kind F13App -ExplicitPath $AppProject

$bootMap = Join-Path $BootProject 'DefaultBuild\RL78_F13_Boot_loader_CAN.map'
$bootHex = Join-Path $BootProject 'DefaultBuild\boot0000_1FFF.hex'
$bootLink = Join-Path $BootProject 'DefaultBuild\RL78_F13_Boot_loader_CAN.clnk'
$bootMain = Join-Path $BootProject 'r_main.c'
$bootUserDef = Join-Path $BootProject 'user_def.h'
$bootConfig = Join-Path $BootProject 'can_boot_config.h'
$bootMetadataConfig = Join-Path $BootProject 'image_metadata_config.h'
$bootHandoff = Join-Path $BootProject 'boot_app_handoff.h'
$appMap = Join-Path $AppProject 'DefaultBuild\RL78_F13_Boot_loader_CAN_app.map'
$appHex = Join-Path $AppProject 'DefaultBuild\flash_app_variable_CRC.hex'
$appBinary = Join-Path $AppProject 'DefaultBuild\RL78_F13_Boot_loader_CAN_app_update.bin'
$appTable = Join-Path $AppProject 'ftable_RL78_F13.asm'
$appTableConfig = Join-Path $AppProject 'ftable.inc'
$appConfig = Join-Path $AppProject 'can_boot_config.h'
$appCustomFunc = Join-Path $AppProject 'custom_func.c'
$appCustomHeader = Join-Path $AppProject 'custom_func.h'
$boardSource = Join-Path $BootProject 'f13_board_support.c'
$appSystemInit = Join-Path $AppProject 'src\code_gen\r_systeminit.c'
$bootProjectFile = Join-Path $BootProject 'RL78_F13_Boot_loader_CAN.mtpj'
$appProjectFile = Join-Path $AppProject 'RL78_F13_Boot_loader_CAN_app.mtpj'

$requiredFiles = @(
    $bootMap, $bootHex, $bootLink, $bootMain, $bootUserDef, $bootConfig,
    $bootMetadataConfig, $bootHandoff,
    $appMap, $appHex, $appBinary, $appTable, $appTableConfig, $appConfig,
    $appCustomFunc, $appCustomHeader, $boardSource, $appSystemInit,
    $bootProjectFile, $appProjectFile
)
foreach ($path in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required artifact is missing: $path"
    }
}

function Get-EquValue([string]$text, [string]$name) {
    $match = [regex]::Match(
        $text,
        ('(?m)^\s*{0}\s+\.EQU\s+(0x[0-9A-Fa-f]+|[0-9]+)\s*(?:;.*)?$' -f
            [regex]::Escape($name)))
    if (-not $match.Success) { throw "Missing .EQU value: $name" }
    $raw = $match.Groups[1].Value
    if ($raw.StartsWith('0x', [StringComparison]::OrdinalIgnoreCase)) {
        return [Convert]::ToUInt32($raw.Substring(2), 16)
    }
    return [Convert]::ToUInt32($raw, 10)
}

$appTableConfigText = [System.IO.File]::ReadAllText($appTableConfig)
$appStart = Get-EquValue $appTableConfigText 'FLASH_TABLE'
$jumpTableSize = Get-EquValue $appTableConfigText 'INTERRUPT_OFFSET'
$manifestAddress = $appStart + $jumpTableSize
$formatInfo = Get-EquValue $appTableConfigText 'IMAGE_MANIFEST_FORMAT_INFO'
$protocolInfo = Get-EquValue $appTableConfigText 'IMAGE_MANIFEST_PROTOCOL'
$targetInfo = Get-EquValue $appTableConfigText 'IMAGE_MANIFEST_TARGET'
$manifestSize = ($formatInfo -shr 8) -band 0xFF
$metadataRegionSize = Get-EquValue $appTableConfigText 'IMAGE_METADATA_REGION_SIZE'
$appCodeStart = $appStart + 0x200
$minimumUpdateSize = Get-EquValue $appTableConfigText 'MINIMUM_IMAGE_SIZE'
$maximumUpdateSize = Get-EquValue $appTableConfigText 'APP_REGION_SIZE'
$codeFlashSize = Get-EquValue $appTableConfigText 'CODE_FLASH_SIZE'
$flashEnd = $codeFlashSize - 1
$targetId = $targetInfo -band 0xFFFF
$protocolId = $protocolInfo -band 0xFFFF
$protocolVersion = ($protocolInfo -shr 16) -band 0xFF
$imageFormat = $formatInfo -band 0xFF
$checksumType = ($protocolInfo -shr 24) -band 0xFF
$manifestMagic = Get-EquValue $appTableConfigText 'IMAGE_MANIFEST_MAGIC'
$manifestSectionName = '.appmeta_AT{0:X4}' -f $manifestAddress
$jumpSectionName = '.jtext_AT{0:X4}' -f $appStart

$failures = [System.Collections.Generic.List[string]]::new()
function Pass([string]$message) {
    Write-Host "[PASS] $message"
}
function Fail([string]$message) {
    $script:failures.Add($message)
    Write-Host "[FAIL] $message" -ForegroundColor Red
}
function Require([bool]$condition, [string]$message) {
    if ($condition) { Pass $message } else { Fail $message }
}

function Read-IntelHex([string]$path) {
    $memory = @{}
    [uint32]$baseAddress = 0
    $sawEof = $false
    foreach ($rawLine in [System.IO.File]::ReadAllLines($path)) {
        $line = $rawLine.Trim()
        if ($line.Length -eq 0) { continue }
        if (-not $line.StartsWith(':') -or (($line.Length - 1) % 2) -ne 0) {
            throw "Invalid Intel HEX line in ${path}: $line"
        }
        $bytes = [System.Collections.Generic.List[byte]]::new()
        for ($index = 1; $index -lt $line.Length; $index += 2) {
            $bytes.Add([Convert]::ToByte($line.Substring($index, 2), 16))
        }
        $sum = 0
        foreach ($byte in $bytes) { $sum = ($sum + $byte) -band 0xFF }
        if ($sum -ne 0) { throw "Intel HEX checksum failure in $path" }
        $count = $bytes[0]
        if ($bytes.Count -ne ($count + 5)) {
            throw "Intel HEX byte count failure in $path"
        }
        $offset = ([uint32]$bytes[1] -shl 8) -bor $bytes[2]
        $type = $bytes[3]
        switch ($type) {
            0 {
                for ($dataIndex = 0; $dataIndex -lt $count; $dataIndex++) {
                    $address = [uint32]($baseAddress + $offset + $dataIndex)
                    if ($memory.ContainsKey($address)) {
                        throw ('Duplicate Intel HEX address 0x{0:X} in {1}' -f $address, $path)
                    }
                    $memory[$address] = $bytes[4 + $dataIndex]
                }
            }
            1 { $sawEof = $true }
            2 {
                $baseAddress = [uint32]((($bytes[4] -shl 8) -bor $bytes[5]) -shl 4)
            }
            4 {
                $baseAddress = [uint32]((($bytes[4] -shl 8) -bor $bytes[5]) -shl 16)
            }
            3 { }
            5 { }
            default { throw "Unsupported Intel HEX record type $type in $path" }
        }
    }
    if (-not $sawEof) { throw "Intel HEX EOF record is missing: $path" }
    return $memory
}

function Read-U16([byte[]]$data, [int]$offset) {
    return [uint16]([uint32]$data[$offset] -bor
        ([uint32]$data[$offset + 1] -shl 8))
}
function Read-U32([byte[]]$data, [int]$offset) {
    [uint64]$value = [uint64]$data[$offset]
    $value = $value -bor ([uint64]$data[$offset + 1] -shl 8)
    $value = $value -bor ([uint64]$data[$offset + 2] -shl 16)
    $value = $value -bor ([uint64]$data[$offset + 3] -shl 24)
    return [uint32]$value
}
function Read-Ascii([byte[]]$data, [int]$offset, [int]$length) {
    $bytes = [System.Collections.Generic.List[byte]]::new()
    for ($index = 0; $index -lt $length; $index++) {
        if ($data[$offset + $index] -eq 0) { break }
        $bytes.Add($data[$offset + $index])
    }
    return [Text.Encoding]::ASCII.GetString($bytes.ToArray())
}
function Get-Crc32([byte[]]$data, [int]$length) {
    [uint32]$crc = [uint32]::MaxValue
    [uint32]$polynomial = 3988292384
    for ($index = 0; $index -lt $length; $index++) {
        $crc = [uint32]($crc -bxor $data[$index])
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 1) -ne 0) {
                $crc = [uint32](([uint64]$crc -shr 1) -bxor $polynomial)
            } else {
                $crc = [uint32]([uint64]$crc -shr 1)
            }
        }
    }
    return [uint32]($crc -bxor [uint32]::MaxValue)
}

function Get-Section([string]$mapText, [string]$name) {
    $escaped = [regex]::Escape($name)
    $match = [regex]::Match(
        $mapText,
        "(?m)^$escaped\s*\r?\n\s*([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]+)")
    if (-not $match.Success) { return $null }
    return [pscustomobject]@{
        Start = [Convert]::ToUInt32($match.Groups[1].Value, 16)
        End = [Convert]::ToUInt32($match.Groups[2].Value, 16)
        Size = [Convert]::ToUInt32($match.Groups[3].Value, 16)
    }
}

function Get-MapSymbolValue([string]$mapText, [string]$name) {
    $escaped = [regex]::Escape($name)
    $match = [regex]::Match(
        $mapText,
        "(?m)^\s*$escaped\s*\r?\n\s*([0-9A-Fa-f]{8})\b")
    if (-not $match.Success) { return $null }
    return [Convert]::ToUInt32($match.Groups[1].Value, 16)
}

function Get-HexMacro([string]$text, [string]$name) {
    $match = [regex]::Match(
        $text,
        ('(?m)^\s*#define\s+{0}\s+\(0x([0-9A-Fa-f]+)[UuLl]*\)' -f
            [regex]::Escape($name)))
    if (-not $match.Success) { return $null }
    return [Convert]::ToUInt32($match.Groups[1].Value, 16)
}

$bootMapText = [System.IO.File]::ReadAllText($bootMap)
$bootConfigText = (@($bootConfig,$bootMetadataConfig,$bootHandoff) |
    ForEach-Object { [IO.File]::ReadAllText($_) }) -join "`n"
$sharedRamAddress = Get-HexMacro $bootConfigText 'BOOT_APP_SHARED_RAM_ADDRESS'
$bootSections = @('.btext', '.btextf', '.text', '.SLIB', '.textf', '.constf',
                  '.data', 'FSL_FCD', 'FSL_RCD', 'FSL_BCD', 'FSL_BECD')
$maximumBootEnd = 0
foreach ($name in $bootSections) {
    $section = Get-Section $bootMapText $name
    if ($null -eq $section) {
        Fail "Boot section $name exists in the map"
        continue
    }
    if ($section.Size -gt 0 -and $section.End -gt $maximumBootEnd) {
        $maximumBootEnd = $section.End
    }
}
Require ($maximumBootEnd -lt $appStart) (
    'Boot/FSL maximum end 0x{0:X4} is below APP_FLASH_START 0x{1:X4}' -f
        $maximumBootEnd, $appStart)
Require (($appStart - ($maximumBootEnd + 1)) -ge 0x200) (
    'Boot/FSL retains at least 0x200 bytes of growth margin (current 0x{0:X})' -f
        ($appStart - ($maximumBootEnd + 1)))
$bootSharedSection = Get-Section $bootMapText '.bss_ATFF710'
$bootSharedSymbol = Get-MapSymbolValue $bootMapText '__no_init_global'
Require ($sharedRamAddress -eq 0xFF710 -and
         $null -ne $bootSharedSection -and
         $bootSharedSection.Start -eq $sharedRamAddress -and
         $bootSharedSection.Size -eq 0x10 -and
         $bootSharedSymbol -eq $sharedRamAddress) `
    'Boot no-init handoff RAM is exactly 0xFF710-0xFF71F'

$bootMemory = Read-IntelHex $bootHex
$bootAddresses = @($bootMemory.Keys | ForEach-Object { [uint32]$_ })
Require (($bootAddresses | Measure-Object -Minimum).Minimum -eq 0) `
    'Boot divided HEX begins at 0x0000'
Require (($bootAddresses | Measure-Object -Maximum).Maximum -lt $appStart) `
    'Boot divided HEX contains no application addresses'

$appMapText = [System.IO.File]::ReadAllText($appMap)
$appSharedSection = Get-Section $appMapText '.bss_ATFF710'
$appSharedSymbol = Get-MapSymbolValue $appMapText '_boot_shared_ram'
Require ($null -ne $appSharedSection -and
         $appSharedSection.Start -eq $sharedRamAddress -and
         $appSharedSection.Size -eq 0x10 -and
         $appSharedSymbol -eq $sharedRamAddress) `
    'Application shares the exact 0xFF710-0xFF71F no-init RAM contract'
$jumpSection = Get-Section $appMapText $jumpSectionName
$manifestSection = Get-Section $appMapText $manifestSectionName
$codeSection = Get-Section $appMapText '.const'
Require ($null -ne $jumpSection -and $jumpSection.Start -eq $appStart -and
         $jumpSection.Size -eq $jumpTableSize) `
    ('Application jump table is 0x{0:X}-0x{1:X}' -f
        $appStart, ($manifestAddress - 1))
Require ($null -ne $manifestSection -and
         $manifestSection.Start -eq $manifestAddress -and
         $metadataRegionSize -eq 256 -and $manifestSection.Size -eq $metadataRegionSize) `
    ('Application metadata region is {0} bytes, core{1}, at0x{2:X}' -f
        $metadataRegionSize, $manifestSize, $manifestAddress)
Require ($null -ne $codeSection -and $codeSection.Start -eq $appCodeStart) `
    ('Application linked code/data begins at configured 0x{0:X}' -f
        $appCodeStart)

$loadedEndSymbols = @(
    ('__e.{0}' -f $jumpSectionName.Substring(1)),
    ('__e.{0}' -f $manifestSectionName.Substring(1)),
    '__e.const', '__e.text',
    '__e.RLIB', '__e.SLIB', '__e.textf', '__e.constf', '__e.data',
    '__e.sdata', '__e.init_array'
)
$linkedEnd = 0
foreach ($symbol in $loadedEndSymbols) {
    $value = Get-MapSymbolValue $appMapText $symbol
    if ($null -ne $value -and $value -gt $linkedEnd) {
        $linkedEnd = $value
    }
}
Require ($linkedEnd -ge $appCodeStart -and
         $linkedEnd -le ($flashEnd - 3)) `
    ('Application map-derived linked end 0x{0:X5} fits code flash' -f
        $linkedEnd)

[byte[]]$image = [System.IO.File]::ReadAllBytes($appBinary)
$updateSize = $image.Length
$crcOffset = $updateSize - 4
$crcAddress = $appStart + $crcOffset
$updateEnd = $appStart + $updateSize - 1
$expectedCrcAddress = [uint32](($linkedEnd + 3) -band -4)
Require ($updateSize -ge $minimumUpdateSize -and
         $updateSize -le $maximumUpdateSize -and
         ($updateSize -band 3) -eq 0) `
    ('Application update BIN length 0x{0:X} is aligned and within 0x{1:X}-0x{2:X}' -f
        $updateSize, $minimumUpdateSize, $maximumUpdateSize)
Require ($crcAddress -eq $expectedCrcAddress) `
    ('CRC address 0x{0:X5} follows aligned map linked end 0x{1:X5}' -f
        $crcAddress, $linkedEnd)

$appMemory = Read-IntelHex $appHex
$appAddresses = @($appMemory.Keys | ForEach-Object { [uint32]$_ })
Require (($appAddresses | Measure-Object -Minimum).Minimum -eq $appStart) `
    'Application update HEX begins at APP_FLASH_START'
Require (($appAddresses | Where-Object { $_ -lt $appStart }).Count -eq 0) `
    'Application update HEX contains no data below APP_FLASH_START'
Require ($appMemory.ContainsKey([uint32]$crcAddress) -and
         $appMemory.ContainsKey([uint32]$updateEnd)) `
    ('Application update HEX contains all four CRC bytes at 0x{0:X5}-0x{1:X5}' -f
        $crcAddress, $updateEnd)
Require (($appAddresses | Measure-Object -Maximum).Maximum -eq $updateEnd -and
         $appMemory.Count -eq $updateSize) `
    'Application update HEX is contiguous and ends at the variable image end'

$manifestOffset = $manifestAddress - $appStart
$storedMagic = Read-U32 $image $manifestOffset
$storedFormat = $image[$manifestOffset + 4]
$storedLength = $image[$manifestOffset + 5]
$storedFlags = Read-U16 $image ($manifestOffset + 6)
$storedProtocolId = Read-U16 $image ($manifestOffset + 8)
$storedProtocolVersion = $image[$manifestOffset + 10]
$storedChecksumType = $image[$manifestOffset + 11]
$storedTarget = Read-U16 $image ($manifestOffset + 12)
$storedCanMode = $image[$manifestOffset + 14]
$storedCanIdFormat = $image[$manifestOffset + 15]
$storedVersion = Read-U32 $image ($manifestOffset + 16)
$storedBuild = Read-U32 $image ($manifestOffset + 20)
$storedSize = Read-U32 $image ($manifestOffset + 24)
$storedAppStart = Read-U32 $image ($manifestOffset + 28)
$storedCapacity = Read-U32 $image ($manifestOffset + 32)
$storedCodeFlash = Read-U32 $image ($manifestOffset + 36)
$storedBootSize = Read-U32 $image ($manifestOffset + 40)
$storedEraseSize = Read-U32 $image ($manifestOffset + 44)
$storedNominalMask = Read-U32 $image ($manifestOffset + 48)
$storedDataMask = Read-U32 $image ($manifestOffset + 52)
$storedRequestId = Read-U32 $image ($manifestOffset + 56)
$storedResponseId = Read-U32 $image ($manifestOffset + 60)
$storedPart = Read-Ascii $image ($manifestOffset + 64) 16
$storedFamily = Read-Ascii $image ($manifestOffset + 80) 12
$storedMinimumSize = Read-U32 $image ($manifestOffset + 92)
$storedCrc = Read-U32 $image $crcOffset
$extensionErased = $true
for ($i=$manifestSize; $i -lt $metadataRegionSize; $i++) {
    if ($image[$manifestOffset+$i] -ne 255) { $extensionErased=$false }
}
Require $extensionErased 'All128 reserved metadata extension bytes are0xFF'
Require ((Read-U32 $image ($manifestOffset+96)) -eq 0x31445055 -and
    (Read-U32 $image ($manifestOffset+100)) -eq (Get-HexMacro $bootConfigText 'IMAGE_POLICY_WORD') -and
    (Read-U32 $image ($manifestOffset+104)) -eq 0x31563146 -and
    (Read-U32 $image ($manifestOffset+108)) -le (Get-HexMacro $bootConfigText 'IMAGE_BOOT_VERSION') -and
    (Read-U32 $image ($manifestOffset+116)) -le 1) `
    'UPD1 policy: DEVICE_APP/CAN, fixed F13 IRQ ABI and supported minimum boot'
$calculatedCrc = Get-Crc32 $image ($image.Length - 4)
Require ($storedMagic -eq $manifestMagic -and $storedFormat -eq $imageFormat -and
         $storedLength -eq $manifestSize -and $storedFlags -eq 3 -and
         $storedProtocolId -eq $protocolId -and
         $storedProtocolVersion -eq $protocolVersion -and
         $storedChecksumType -eq $checksumType -and
         $storedTarget -eq $targetId -and $storedVersion -ne 0xFFFFFFFF -and
         $storedBuild -ne 0xFFFFFFFF -and $storedSize -eq $updateSize) `
    ('Manifest identity is valid (target=0x{0:X4}, protocol={1}.{2}, version=0x{3:X8}, build={4}, size=0x{5:X})' -f
        $storedTarget, $storedProtocolId, $storedProtocolVersion,
        $storedVersion, $storedBuild, $storedSize)
Require ($storedAppStart -eq $appStart -and
         $storedCapacity -eq $maximumUpdateSize -and
         $storedCodeFlash -eq $codeFlashSize -and
         $storedBootSize -eq (Get-EquValue $appTableConfigText 'BOOT_RESERVED_SIZE') -and
         $storedEraseSize -eq (Get-EquValue $appTableConfigText 'FLASH_ERASE_BLOCK_SIZE') -and
         $storedMinimumSize -eq $minimumUpdateSize) `
    ('Manifest flash layout is self-consistent (start=0x{0:X}, capacity=0x{1:X}, flash=0x{2:X})' -f
        $storedAppStart, $storedCapacity, $storedCodeFlash)
Require ($storedCanMode -ne 0 -and $storedCanIdFormat -le 1 -and
         $storedNominalMask -ne 0 -and
         $storedRequestId -ne $storedResponseId -and
         -not [string]::IsNullOrWhiteSpace($storedPart) -and
         -not [string]::IsNullOrWhiteSpace($storedFamily)) `
    ('Manifest CAN/MCU metadata is valid ({0} {1}, IDs 0x{2:X}/0x{3:X}, masks 0x{4:X}/0x{5:X})' -f
        $storedFamily, $storedPart, $storedRequestId, $storedResponseId,
        $storedNominalMask, $storedDataMask)
Require ($storedCrc -eq $calculatedCrc) `
    ('Stored CRC32 matches calculated CRC32 0x{0:X8}' -f $calculatedCrc)

$appConfigText = [System.IO.File]::ReadAllText($appConfig)
$appUsesBootConfig = $appConfigText -match
    '#include\s+"\.\./RL78_F13_Boot_loader_CAN/can_boot_config\.h"'
$bootTarget = Get-HexMacro $bootConfigText 'IMAGE_TARGET_ID'
$bootProtocol = Get-HexMacro $bootConfigText 'CAN_BOOT_PROTOCOL_VERSION'
$bootImageFormat = Get-HexMacro $bootConfigText 'IMAGE_FORMAT_VERSION'
$appIdentityText = [System.IO.File]::ReadAllText((Join-Path $AppProject 'app_identity.h'))
$appTarget = if ($appUsesBootConfig) {
    $bootTarget
} else {
    Get-HexMacro $appConfigText 'IMAGE_TARGET_ID'
}
$appProtocol = if ($appUsesBootConfig) {
    $bootProtocol
} else {
    Get-HexMacro $appConfigText 'CAN_BOOT_PROTOCOL_VERSION'
}
$appIdentityVersion = if ($appUsesBootConfig) {
    Get-HexMacro $appIdentityText 'CAN_BOOT_APP_FIRMWARE_VERSION'
} else {
    Get-HexMacro $appConfigText 'CAN_BOOT_APP_FIRMWARE_VERSION'
}
Require $appUsesBootConfig `
    'Application INFO identity includes the authoritative boot configuration'
Require ([IO.File]::ReadAllText((Join-Path $AppProject 'image_metadata_config.h')) -match
    '#include\s+"\.\./RL78_F13_Boot_loader_CAN/image_metadata_config\.h"') `
    'Application metadata includes its paired image contract independently of CAN'
Require ($bootTarget -eq $targetId -and $appTarget -eq $targetId -and
         $bootProtocol -eq $protocolVersion -and
         $appProtocol -eq $protocolVersion -and
         $bootImageFormat -eq $imageFormat) `
    'Boot/app INFO target, protocol, and image format match the update profile'
Require ($appIdentityVersion -eq $storedVersion -and
         $bootConfigText -notmatch '#define\s+CAN_BOOT_APP_FIRMWARE_VERSION') `
    'App-local INFO identity matches manifest without a boot app-version dependency'

& python (Join-Path $repoRoot 'tests\test_f13_irq_contract.py') --boot-project $BootProject --app-project $AppProject
Require ($LASTEXITCODE -eq 0) 'Complete normal-vector / RETI / real HEX stack-model / app-only mapping tests'
$linkText = [System.IO.File]::ReadAllText($bootLink)
Require ($linkText -notmatch '-VECTN=') 'No legacy linker vector ownership competes with assembly'
$bootMainText = [System.IO.File]::ReadAllText($bootMain)
$appCustomText = [System.IO.File]::ReadAllText($appCustomFunc)
$appCustomHeaderText = [System.IO.File]::ReadAllText($appCustomHeader)
Require ($bootMainText -match '#pragma interrupt f13_boot_timer_isr' -and
         $bootMainText -notmatch 'APP_INTTRJ0_ENTRY_ADDRESS|vect\s*=\s*INTTRJ0') `
    'Shared timer is a normal boot ISR, not a C-call app callback'
Require ($bootMainText -match
             'BOOT_APP_RAM_HANDOFF_OFFSET[\s\S]*BOOT_APP_HANDOFF_SIGNATURE' -and
         $bootMainText -match
             'BOOT_APP_RAM_UPDATE_OFFSET[\s\S]*BOOT_APP_UPDATE_COMPLETE_SIGNATURE' -and
         $appCustomText -match 'AppSharedRead32\(BOOT_APP_RAM_HANDOFF_OFFSET\)' -and
         $appCustomText -match 'AppSharedRead32\(BOOT_APP_RAM_UPDATE_OFFSET\)' -and
         $appCustomText -match 'AppSharedWrite32\(BOOT_APP_RAM_HANDOFF_OFFSET, 0UL\)' -and
         $appCustomText -match 'AppSharedWrite32\(BOOT_APP_RAM_UPDATE_OFFSET, 0UL\)') `
    'Boot writes and app consumes one-shot handoff/update signatures'
Require ($appCustomHeaderText -match
             '#define\s+APP_RUNTIME_LOG_TAG\s+"[^"]+"' -and
         $appCustomText -match
             'log\(timer\)-"\s+APP_RUNTIME_LOG_TAG') `
    'Application runtime log includes the configurable image tag'
Require ($appTableConfig -and
         ([System.IO.File]::ReadAllText($appTableConfig) -match
          'APP_IMAGE_VERSION\s+\.EQU\s+0x([0-9A-Fa-f]{8})') -and
         [Convert]::ToUInt32($Matches[1], 16) -eq $storedVersion) `
    'ftable.inc APP_IMAGE_VERSION matches the generated manifest'

$appSystemText = [System.IO.File]::ReadAllText($appSystemInit)
Require ($appSystemText -match '#define\s+R_CGC_Create\s+F13_BoardClockPrepare') `
    'App generated-user startup hook retains the shared X1 clock preparation'
Require ($bootMainText -match 'F13_BoardClockPrepare\(\)' -and
         $bootMainText -match 'F13_BoardClockStart\(\)' -and
         $appCustomText -match 'F13_BoardClockStart\(\)' -and
         $appCustomText -match 'F13_BoardCanPrepare\(\)') `
    'Boot/app invoke shared clock startup and CAN handoff preparation'
foreach ($projectFile in @($bootProjectFile, $appProjectFile)) {
    [xml]$project = [System.IO.File]::ReadAllText($projectFile)
    $registered = @($project.SelectNodes('//Instance[Name="f13_board_support.c"]'))
    Require ($registered.Count -eq 1) `
        ('Shared board support is registered once in {0}' -f
            [System.IO.Path]::GetFileName($projectFile))
}

if ($failures.Count -ne 0) {
    Write-Host "F13 artifact verification failed with $($failures.Count) error(s)." -ForegroundColor Red
    exit 1
}

$summary = ('F13 artifact verification passed. Boot/FSL end=0x{0:X4}, ' +
            'image size=0x{1:X}, version=0x{2:X8}, CRC32=0x{3:X8}.') -f
            $maximumBootEnd, $updateSize, $storedVersion, $storedCrc
Write-Host $summary -ForegroundColor Green
