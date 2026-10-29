param([string]$F13App='', [string]$F24App='', [string]$F24FdApp='',
      [string]$DeviceRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
if($DeviceRoot){
    . (Join-Path $PSScriptRoot 'device_app_catalog.ps1')
    $targets=@(Get-DeviceTimingApps $DeviceRoot|ForEach-Object {$_.Project})
}else{$targets=@(
    (Get-CompanionPath -Kind F13App -ExplicitPath $F13App),
    (Get-CompanionPath -Kind F24App -ExplicitPath $F24App),
    (Get-CompanionPath -Kind F24FdApp -ExplicitPath $F24FdApp))}
$jobs=@(@('device_test_bins.ps1','generate_test_bins.ps1'),@('device_test_bins.bat','generate_test_bins.bat'),
        @('device_test_bins_output.ps1','device_test_bins_output.ps1'))
foreach($target in $targets){if(!(Test-Path -LiteralPath $target -PathType Container)){throw 'Missing App project.'}}
foreach($job in $jobs){
    $bytes=[IO.File]::ReadAllBytes((Join-Path $PSScriptRoot $job[0]))
    foreach($target in $targets){
        $path=Join-Path $target $job[1]
        if($CheckOnly){
            if(!(Test-Path -LiteralPath $path) -or [Convert]::ToBase64String([IO.File]::ReadAllBytes($path)) -cne [Convert]::ToBase64String($bytes)){
                throw ('App test-BIN helper differs: '+$path)
            }
        }else{[IO.File]::WriteAllBytes($path,$bytes)}
    }
}
Write-Host ('PASS: '+$targets.Count+' App-local timing-BIN helper sets match the canonical scripts.')
