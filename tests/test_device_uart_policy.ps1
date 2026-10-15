$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\scripts\image_uart_policy.ps1')
foreach ($rate in @(115200,460800,921600)) {
    $metadata=New-Object byte[] 256
    for ($i=128; $i -lt 256; $i++) { $metadata[$i]=255 }
    [BitConverter]::GetBytes([uint32]0x31445055).CopyTo($metadata,96)
    $metadata[100]=1; $metadata[101]=32; $metadata[102]=1; $metadata[103]=4
    [BitConverter]::GetBytes([uint32]0x31563146).CopyTo($metadata,104)
    Set-DeviceUartImagePolicy -Metadata $metadata -UartUpdateBaud $rate
    if ($metadata[100] -ne 2 -or [BitConverter]::ToUInt32($metadata,112) -ne $rate) { throw 'Wrong fixed baud emitted.' }
    $metadata[102]=2
    $failed=$false
    try { Set-DeviceUartImagePolicy -Metadata $metadata -UartUpdateBaud $rate } catch { $failed=$true }
    if (!$failed) { throw 'Host bridge image mistaken for device UART.' }
}
'PASS: build-time DEVICE UART baud115200/460800/921600 and host-role rejection; no files/flash written.'
