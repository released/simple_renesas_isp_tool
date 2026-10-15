# Build-time helper for future DEVICE UART packagers, before final image CRC.
# Boot and app must consume the same target-project configuration. Never take
# this value from the PC Host Link combo or silently rewrite a loaded BIN.
function Set-DeviceUartImagePolicy {
    param([Parameter(Mandatory=$true)][byte[]]$Metadata,
          [Parameter(Mandatory=$true)][uint32]$UartUpdateBaud)
    if ($Metadata.Length -ne 256 -or [BitConverter]::ToUInt32($Metadata,96) -ne 0x31445055 -or
        $Metadata[100] -notin @(1,2) -or $Metadata[101] -ne 32 -or $Metadata[102] -ne 1 -or
        -not ($Metadata[103] -band 4) -or ($Metadata[103] -band 0xF8) -or
        $UartUpdateBaud -eq 0 -or $UartUpdateBaud -eq [uint32]::MaxValue) {
        throw 'A DEVICE UART image requires valid256-byte metadata and a compile-time nonzero UART update baud.'
    }
    $abi=[BitConverter]::ToUInt32($Metadata,104)
    if (!$abi -or $abi -eq [uint32]::MaxValue -or
        [BitConverter]::ToUInt32($Metadata,108) -eq [uint32]::MaxValue) { throw 'Invalid IRQ ABI / minimum boot version.' }
    for ($i=116; $i -lt 128; $i++) { if ($Metadata[$i]) { throw 'Unknown UART policy field.' } }
    for ($i=128; $i -lt 256; $i++) { if ($Metadata[$i] -ne 255) { throw 'Metadata reserve must beFF.' } }
    $Metadata[100]=2
    [BitConverter]::GetBytes($UartUpdateBaud).CopyTo($Metadata,112)
}
