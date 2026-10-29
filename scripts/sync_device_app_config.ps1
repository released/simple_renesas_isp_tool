param([string]$F13Boot='', [string]$F24Boot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$F13Boot=Get-CompanionPath -Kind F13Boot -ExplicitPath $F13Boot
if(!$F24Boot) { $F24Boot=Join-Path (Split-Path -Parent $F13Boot) 'RL78_F24_Boot_loader_CAN' }
$jobs=@(
    @('device_app_config.ps1','app_config.ps1'),
    @('device_app_report.ps1','app_build_report.ps1'))
foreach($job in $jobs) {
    $source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot $job[0]))
    $targets=@()
    $deviceRoot=Split-Path -Parent $F13Boot
    foreach($name in @('RL78_F13_Boot_loader_CAN','RL78_F13_Boot_loader_CAN_app','RL78_F13_CAN_Transfer',
        'RL78_F24_Boot_loader_CAN','RL78_F24_Boot_loader_CAN_app','RL78_F24_CAN_Transfer',
        'RL78_F24_Boot_loader_CAN_FD','RL78_F24_Boot_loader_CAN_FD_app','RL78_F24_CAN_FD_Transfer')) {
        $root=Join-Path $deviceRoot $name
        if($name -eq 'RL78_F13_Boot_loader_CAN'){$root=$F13Boot}
        if($name -eq 'RL78_F24_Boot_loader_CAN'){$root=$F24Boot}
        if(!(Test-Path -LiteralPath $root)){continue}
        $relative=if($name.StartsWith('RL78_F13_')){$job[1]}else{'tools\'+$job[1]}
        $targets+=Join-Path $root $relative
    }
    foreach($target in $targets) {
        if(!(Test-Path -LiteralPath (Split-Path -Parent $target) -PathType Container)) { throw "Missing Boot source directory: $target" }
        if($CheckOnly -and (!(Test-Path -LiteralPath $target) -or [IO.File]::ReadAllText($target).Replace("`r`n","`n") -cne $source.Replace("`r`n","`n"))) {
            throw "Golden configuration/report helper differs: $target"
        }
    }
    if(!$CheckOnly) {
        foreach($target in $targets) { [IO.File]::WriteAllText($target,$source,(New-Object Text.UTF8Encoding($false))) }
    }
}
Write-Host 'PASS: F13/F24 local C-header readers and build-report helpers match golden sources.'
