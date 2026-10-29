param([string]$BootProject='',
    [string]$AppProject,[string]$TransferProject,[ValidateSet('boot','app','transfer','all')][string]$Kind='all',[switch]$Prepare)
$ErrorActionPreference='Stop'
$targets=@();if($Kind -in @('boot','all')){if(!$BootProject){throw 'Explicit BootProject required.'};$targets+=@{Root=(Resolve-Path -LiteralPath $BootProject).Path;Kind='boot'}}
if($Kind -in @('app','all')){if(!$AppProject){throw 'Explicit AppProject required.'};$targets+=@{Root=(Resolve-Path -LiteralPath $AppProject).Path;Kind='app'}}
if($Kind -eq 'transfer' -or ($Kind -eq 'all' -and $TransferProject)){if(!$TransferProject){throw 'Explicit TransferProject required.'};$targets+=@{Root=(Resolve-Path -LiteralPath $TransferProject).Path;Kind='transfer'}}
$logs=Join-Path (Split-Path -Parent $PSScriptRoot) 'tmp\internal\rh850'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$compiler=Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\CS+\CC\CubeSuite+.exe'
foreach($t in $targets){
    $root=$t.Root;$name=Split-Path -Leaf $root
    $helper=Join-Path $root 'tools\device_build.ps1'
    if(!(Test-Path -LiteralPath $helper)){throw 'Missing project-local build tools.'}
    if($Prepare){& $helper -Action prepare -Project $root -Kind $t.Kind}
    $log=Join-Path $logs ($name+'_rebuild.log')
    $proc=Start-Process -FilePath $compiler -ArgumentList ('/br "'+(Join-Path $root ($name+'.mtpj'))+'"') -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr') -WindowStyle Hidden -PassThru -Wait
    $result=[IO.File]::ReadAllText($log);$result -split '\r?\n' | Select-Object -Last 18 | Write-Host
    if($result -notmatch 'Build ended\(Error:0, Warning:\d+\)' -or $result -match 'Failed:[1-9]'){throw "RH850 rebuild failed: $log"}
    & $helper -Action post -Project $root -Kind $t.Kind
}
Write-Host 'PASS: selected RH850 builds. Hardware/COM/programming NOT RUN.'
