param(
    [Parameter(Mandatory=$true)][string]$DeviceProject,
    [switch]$Prepare
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $DeviceProject).Path
$project=Join-Path $root 'Sample_Project_RH850_S1_CAN_Transfer.mtpj'
if(!(Test-Path -LiteralPath $project -PathType Leaf)){throw 'Expected the RH850 Classical Transfer project.'}
. (Join-Path $root 'tools\evm_prepare.ps1')
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
Update-EvmClock $root
$logRoot=Join-Path (Split-Path -Parent $PSScriptRoot) 'tmp\internal\rh850'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
if($Prepare){
    # Existing source/linker/SC choices are owned by the selected project.
    Assert-DeviceLocalBindings $root
}

$compiler=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\CS+\CC\CubeSuite+.exe'
if(!(Test-Path -LiteralPath $compiler)){throw 'CS+ CC is required; no additional firmware build runtime is used.'}
$log=Join-Path $logRoot 'can_transfer_rebuild.log'
$proc=Start-Process -FilePath $compiler -ArgumentList ('/br "'+$project+'"') -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr') -WindowStyle Hidden -PassThru -Wait
$result=Get-Content -LiteralPath $log -Raw
$result -split '\r?\n' | Select-Object -Last 14 | Write-Host
if($result -notmatch 'Build ended\(Error:0, Warning:\d+\)' -or $result -match 'Failed:[1-9]'){
    throw "RH850 clean rebuild failed: $log"
}
$hex=Join-Path $root 'DefaultBuild\Sample_Project_RH850_S1_CAN_Transfer.mot'
if(!(Test-Path -LiteralPath $hex)){throw 'No current programmer image.'}
& (Join-Path $PSScriptRoot 'verify_rh850_transfer.ps1') -DeviceProject $root
Write-Host ('PASS: CC-RH rebuild; programmer MOT SHA256 '+(Get-FileHash -LiteralPath $hex).Hash)
Write-Host 'Hardware/COM/Flash tests NOT RUN.'
