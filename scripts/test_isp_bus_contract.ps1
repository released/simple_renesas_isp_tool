$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'tmp\internal\tests\isp_bus_contract';[IO.Directory]::CreateDirectory($out)|Out-Null
$clang=(Get-Command clang.exe -ErrorAction Stop).Source
$include=Join-Path $root 'firmware\common\isp_device'
$source=Join-Path $root 'tests\isp_uart_timing_tests.c'
foreach($clock in @(8000000,16000000,20000000)){foreach($baud in @(115200,123456,250000,500000,1000000,2000000)){
    & $clang --target=x86_64-pc-windows-msvc -std=c90 -pedantic-errors -fsyntax-only ('-I'+$include) ('-DISP_UART_CLOCK_HZ='+$clock+'UL') ('-DISP_UART_BAUD='+$baud+'UL') $source
    if($LASTEXITCODE){throw ('Valid UART timing rejected: '+$clock+'/'+$baud)}
}}
foreach($case in @(@(8000000,1700000),@(16000000,300),@(16000000,1700000),@(20000000,2000001))){
    $old=$ErrorActionPreference
    try{$ErrorActionPreference='Continue';$diagnostics=& $clang --target=x86_64-pc-windows-msvc -std=c90 -fsyntax-only ('-I'+$include) ('-DISP_UART_CLOCK_HZ='+$case[0]+'UL') ('-DISP_UART_BAUD='+$case[1]+'UL') $source 2>&1;$status=$LASTEXITCODE}finally{$ErrorActionPreference=$old}
    if(!$status -or ($diagnostics -join "`n") -notmatch '#error|error:'){throw 'Missing explicit rejection for invalid UART timing.'}
}
Write-Host 'PASS eighteen UART clock/rate compile cases and four range/divisor/error rejections. No physical timing claim.'
$contractSource=Join-Path $root 'tests\isp_bus_contract_tests.c'
$cases=@(
    @{Okay=$true;Args=@('ISP_UART_BAUD=115200UL','ISP_BUS_RATE=115200UL','ISP_POLICY_UART_BAUD=115200UL')},
    @{Okay=$true;Args=@('ISP_BUS_RATE=57600UL','ISP_POLICY_UART_BAUD=57600UL')},
    @{Okay=$false;Args=@('ISP_UART_BAUD=115200UL','ISP_BUS_RATE=57600UL','ISP_POLICY_UART_BAUD=115200UL')},
    @{Okay=$false;Args=@('ISP_UART_BAUD=115200UL','ISP_BUS_RATE=115200UL','ISP_POLICY_UART_BAUD=57600UL')})
foreach($row in @(@(100000,0x20,100000,0x20,$true),@(400000,0x42,400000,0x42,$true),
    @(100000,0,100000,0,$false),@(333333,0x20,333333,0x20,$false),@(100000,0x42,100000,0x20,$false))){
    $cases+=@{Okay=$row[4];Args=@('ISP_I2C','ISP_POLICY_UART_BAUD=0UL',('ISP_BUS_RATE='+$row[0]+'UL'),('ISP_I2C_ADDRESS='+$row[1]+'U'),('ISP_LINK_PARAMETER_0='+$row[2]+'UL'),('ISP_LINK_PARAMETER_1='+$row[3]+'UL'))}
}
foreach($case in $cases){
    $arguments=@($case.Args|ForEach-Object {'-D'+$_})
    $old=$ErrorActionPreference
    try{$ErrorActionPreference='Continue';$messages=& $clang --target=x86_64-pc-windows-msvc -std=c90 -pedantic-errors -fsyntax-only ('-I'+$include) @arguments $contractSource 2>&1;$status=$LASTEXITCODE}finally{$ErrorActionPreference=$old}
    if(($status -eq 0) -ne $case.Okay){throw ('Unexpected UART/I2C contract acceptance: '+($case.Args -join ' ')+($messages -join "`n"))}
}
Write-Host 'PASS nine UART/I2C INFO/metadata/rate/address consistency compile cases.'
