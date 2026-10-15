param([ValidateSet('boot','app')][string]$Kind='boot',[switch]$Prepare,[string]$AppProject='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if ($Prepare) {
    if ($AppProject) { throw 'Prepare regenerates only the reference project; omit it for an independent app.' }
    & (Join-Path $PSScriptRoot 'prepare_f24_bridge_projects.ps1') -Kind $Kind
}
$compiler='C:\Program Files (x86)\Renesas Electronics\CS+\CC\CubeSuite+.exe'
$project=Join-Path $repo ('firmware\RL78_F24_Bridge_'+$Kind+'\RL78_F24_Bridge_'+$Kind+'.mtpj')
if ($Kind -eq 'app') {
    if (!$AppProject) { $AppProject=Join-Path $repo 'firmware\RL78_F24_Bridge_app' }
    . (Join-Path $PSScriptRoot 'f24_app_contract.ps1')
    $appContract=Get-F24AppContract $AppProject
    & (Join-Path $PSScriptRoot 'prepare_f24_app_irq.ps1') -AppProject $appContract.Root
    $project=$appContract.Project
} elseif ($AppProject) { throw 'AppProject is only valid with Kind app.' }
$logRoot=Join-Path $repo 'tmp\internal\build\logs'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
$buildLog=Join-Path $logRoot ('f24_bridge_'+$Kind+'_rebuild.log')
if ($Kind -eq 'app' -and $appContract.Name -ne 'RL78_F24_Bridge_app') {
    $buildLog=Join-Path $logRoot ($appContract.Name+'_rebuild.log')
}
$process=Start-Process -FilePath $compiler -ArgumentList ('/br "'+$project+'"') -RedirectStandardOutput $buildLog -RedirectStandardError ($buildLog+'.stderr') -WindowStyle Hidden -PassThru -Wait
$text=Get-Content -LiteralPath $buildLog -Raw
$text -split '\r?\n' | Select-Object -Last 18 | Write-Host
# CS+ may exit zero even when a compiler fails. Its summary is authoritative.
if ($text -notmatch 'Build ended\(Error:0, Warning:\d+\)' -or $text -match 'Failed:[1-9]') {
    throw "F24 bridge $Kind build failed. See $buildLog"
}
Write-Host "F24 bridge $Kind clean rebuild PASS. Hardware NOT RUN."
& (Join-Path $PSScriptRoot 'package_f24_bridge.ps1') -Kind $Kind -AppProject $AppProject
