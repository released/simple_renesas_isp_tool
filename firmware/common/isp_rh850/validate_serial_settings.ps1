param([Parameter(Mandatory=$true)][string]$BootProject,[Parameter(Mandatory=$true)][string]$Project)
$ErrorActionPreference='Stop'
$header=[IO.File]::ReadAllText((Join-Path $BootProject 'common\isp_transport_config.h'))
function Value([string]$Name,[string]$Text=$header){
    $m=[regex]::Match($Text,'(?m)^#define\s+'+[regex]::Escape($Name)+'\s+\(?(0x[0-9A-Fa-f]+|[0-9]+)[ULul]*\)?')
    if(!$m.Success){throw ('Missing literal serial setting: '+$Name)}
    if($m.Groups[1].Value.StartsWith('0x')){return [Convert]::ToUInt32($m.Groups[1].Value.Substring(2),16)}
    return [uint32]$m.Groups[1].Value
}
$rate=Value ISP_BUS_RATE
if((Value ISP_CONTRACT_ID) -eq 2){
    $address=Value ISP_I2C_ADDRESS
    if($rate -notin @(100000,400000) -or $address -lt 8 -or $address -gt 0x77 -or
       (Value ISP_LINK_PARAMETER_0) -ne $rate -or (Value ISP_LINK_PARAMETER_1) -ne $address -or (Value ISP_POLICY_UART_BAUD) -ne 0){throw 'I2C rate/address/metadata contract mismatch.'}
}else{
    if($rate -lt 300 -or $rate -gt 2000000 -or (Value ISP_POLICY_UART_BAUD) -ne $rate){throw 'UART INFO/metadata baud mismatch.'}
    $clock=[IO.File]::ReadAllText((Join-Path $Project 'src\smc_gen\general\r_cg_cgc.c'))
    if($clock -notmatch 'CKSC_ILINS_CTL\s*=\s*_CGC_RLIN_CLK_SOURCE_PPLLCLK2\s*;' -or
       $clock -notmatch 'CKSC_ILIND_CTL\s*=\s*_CGC_RLIN_CLK_DIVIDER_1\s*;'){throw 'Review the changed SC RLIN source/divider and board clock contract together.'}
    $source=[IO.File]::ReadAllText((Join-Path $Project 'src\smc_gen\Config_UART1\Config_UART1.c'))
    $timing=[regex]::Match($source,'RLN31\.LWBR\s*=\s*_UART_([0-9]+)_SAMPLING\s*\|\s*_UART_PRESCALER_CLOCK_SELECT_([0-9]+)\s*;')
    if(!$timing.Success){throw 'Review generated UART1 sampling/prescaler before building this board adapter.'}
    $divisor=Value _UART1_BAUD_RATE_PRESCALER ([IO.File]::ReadAllText((Join-Path $Project 'src\smc_gen\Config_UART1\Config_UART1.h')))
    # The checked EVM clock projection supplies RLIN PPLLCLK2/1=40MHz.
    $actual=40000000.0/([int]$timing.Groups[1].Value*[int]$timing.Groups[2].Value*($divisor+1))
    if([Math]::Abs($actual-$rate)*100 -gt $rate*2){throw ('UART SC registers do not match metadata/INFO baud '+$rate+'. Update paired Boot/App SC and contract together.')}
    Write-Host ('PASS generated UART1 timing: requested='+$rate+', calculated='+[Math]::Round($actual)+'. UART0 Log unchanged.')
}
