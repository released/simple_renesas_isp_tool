param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$source=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_ISP_Boot_loader_UART'
$out=Join-Path $root ('tmp\internal\tests\rh850_uart_settings_'+[guid]::NewGuid().ToString('N').Substring(0,8))
$files=@('common\isp_transport_config.h','src\smc_gen\Config_UART1\Config_UART1.c','src\smc_gen\Config_UART1\Config_UART1.h','src\smc_gen\general\r_cg_cgc.c')
foreach($file in $files){
    $to=Join-Path $out $file;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to))|Out-Null
    Copy-Item -LiteralPath (Join-Path $source $file) -Destination $to
}
$validator=Join-Path $root 'firmware\common\isp_rh850\validate_serial_settings.ps1'
& $validator -BootProject $out -Project $out
$path=Join-Path $out 'common\isp_transport_config.h'
$body=[IO.File]::ReadAllText($path).Replace('(115200UL)','(57600UL)')
[IO.File]::WriteAllText($path,$body)
$refused=$false
try{& $validator -BootProject $out -Project $out}catch{if($_.Exception.Message -notmatch 'SC registers do not match'){throw};$refused=$true}
if(!$refused){throw 'A metadata-only baud change was not rejected.'}
$path=Join-Path $out 'src\smc_gen\Config_UART1\Config_UART1.h'
$body=[IO.File]::ReadAllText($path).Replace('(0x001CU)','(0x0039U)')
[IO.File]::WriteAllText($path,$body)
& $validator -BootProject $out -Project $out
$path=Join-Path $out 'src\smc_gen\general\r_cg_cgc.c'
$body=[IO.File]::ReadAllText($path).Replace('_CGC_RLIN_CLK_DIVIDER_1','_CGC_RLIN_CLK_DIVIDER_2')
[IO.File]::WriteAllText($path,$body)
$refused=$false
try{& $validator -BootProject $out -Project $out}catch{if($_.Exception.Message -notmatch 'changed SC RLIN source'){throw};$refused=$true}
if(!$refused){throw 'A mismatched clock mux was not rejected.'}
Write-Host 'PASS RH850 default/custom57600 and metadata-only/clock-source mismatch rejection;original files not modified.'
