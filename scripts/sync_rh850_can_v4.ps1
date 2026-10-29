param([Parameter(Mandatory=$true)][string]$DeviceRoot,[string]$V4Project='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$DeviceRoot=(Resolve-Path -LiteralPath $DeviceRoot).Path
if(!$V4Project){$V4Project=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_CAN_FD_MultiCHv4'}
$V4Project=(Resolve-Path -LiteralPath $V4Project).Path
$owned=Join-Path $repo 'firmware\common\rh850_can_v4'
function Sync-One([string]$Source,[string]$Destination) {
    $probe=[IO.Path]::GetFullPath($Destination)
    if(!$probe.StartsWith($project.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Mirror target escaped its existing project.'}
    while($probe.Length -ge $project.Length) {
        if((Test-Path -LiteralPath $probe) -and ((Get-Item -LiteralPath $probe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw ('Reparse mirror target refused: '+$probe)}
        $probe=Split-Path -Parent $probe
    }
    if(!(Test-Path -LiteralPath $Source -PathType Leaf)){throw ('Missing source: '+$Source)}
    if(Test-Path -LiteralPath $Destination) {
        if((Get-FileHash -LiteralPath $Source).Hash -ceq (Get-FileHash -LiteralPath $Destination).Hash){return}
        if([IO.File]::ReadAllText($Source).Replace("`r`n","`n") -ceq [IO.File]::ReadAllText($Destination).Replace("`r`n","`n")){return}
    }
    if($CheckOnly){throw ('RH850 v4 mirror drift: '+$Destination)}
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Destination))|Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination
}
foreach($name in @('Sample_Project_RH850_S1_Boot_loader_CAN','Sample_Project_RH850_S1_Boot_loader_CAN_app','Sample_Project_RH850_S1_Boot_loader_CAN_FD','Sample_Project_RH850_S1_Boot_loader_CAN_FD_app','Sample_Project_RH850_S1_CAN_Transfer','Sample_Project_RH850_S1_CAN_FD_Transfer')) {
    $project=Join-Path $DeviceRoot $name
    if(!(Test-Path -LiteralPath (Join-Path $project ($name+'.mtpj')))){throw ('Missing existing project: '+$name)}
    if((Get-Item -LiteralPath $project).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse project refused.'}
    $common=Join-Path $project 'hal\can'
    foreach($file in @('can_service.c','can_service.h','can_board.h')) {
        Sync-One (Join-Path $V4Project $file) (Join-Path $common $file)
    }
    foreach($file in @('rs_canfd.c','rs_canfd.h','rs_canfd_platform.h','rs_canfd_regs.h','rs_canfd_validate.c')) {
        Sync-One (Join-Path $V4Project ('hal\can\'+$file)) (Join-Path $common $file)
    }
    foreach($file in @('can_board.c','can_config.c','can_config.h','can_user_config.h')) {
        Sync-One (Join-Path $owned $file) (Join-Path $common $file)
    }
    foreach($file in @('rh850_can_ports.c','rh850_can_ports.h')) {Sync-One (Join-Path $owned $file) (Join-Path $common $file)}
    if($name.EndsWith('_CAN_Transfer')) {
        Sync-One (Join-Path $repo 'scripts\verify_rh850_transfer.ps1') (Join-Path $project 'tools\verify_transfer.ps1')
    }
}
Write-Host 'PASS RH850 CAN v4 service/driver and ISP adapters mirrored; authoring/SC/startup/protocol inputs preserved.'
