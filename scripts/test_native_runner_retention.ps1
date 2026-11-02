[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'runner_retention' -ShortPath
$records=@()
try {
 foreach($name in @('test_app_restart_timing.ps1','test_bridge_uart.ps1','test_isp_client.ps1')) {
  foreach($mode in @('Failure','Cancel')) {
   $original=Join-Path $PSScriptRoot $name
   $source=[IO.File]::ReadAllText($original)
   if($source -notmatch 'Remove-OwnedTestWorkspace'){throw 'Runner does not own final retirement.'}
   $source=$source.Replace('$PSScriptRoot',("'"+$PSScriptRoot.Replace("'","''")+"'"))
   $failure=if($mode -eq 'Cancel'){'throw [System.Management.Automation.PipelineStoppedException]::new()'}else{"throw 'Owned injected compiler failure'"}
   $replacement='[IO.File]::WriteAllBytes((Join-Path $scope.Path ''owned_payload.bin''),[byte[]](1,2,3,4)); '+$failure
   $pattern=[regex]::new('(?m)^\s*& cl[^\r\n]*')
   if(!$pattern.IsMatch($source)){throw 'No bounded compiler injection point.'}
   $source=$pattern.Replace($source,[Text.RegularExpressions.MatchEvaluator]{param($match)$replacement},1)
   $fixture=Join-Path $scope.Path ($mode+'_'+$name)
   [IO.File]::WriteAllText($fixture,$source)
   $before=@(Get-ChildItem -LiteralPath (Join-Path $root 'tmp\internal\tests') -Directory|ForEach-Object FullName)
   $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$fixture+'"'))
   if($name -eq 'test_isp_client.ps1'){$args+=@('-DeviceRoot',('"'+$DeviceRoot+'"'),'-Mcu','F13','-Bus','UART')}
   $child=Start-Process -FilePath powershell.exe -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scope.Path 'child.stdout') -RedirectStandardError (Join-Path $scope.Path 'child.stderr')
   $null=$child.Handle
   try {
    if(!$child.WaitForExit(30000)){Stop-Process -Id $child.Id;throw 'Owned retention fixture timeout.'}
    $child.WaitForExit()
    $child.Refresh()
    if($null -eq $child.ExitCode){throw 'Child exit status is unavailable.'}
    if($mode -eq 'Failure' -and !$child.ExitCode){throw 'Failure injection did not fail.'}
    $after=@(Get-ChildItem -LiteralPath (Join-Path $root 'tmp\internal\tests') -Directory|ForEach-Object FullName)
    $remaining=@($after|Where-Object{$_ -notin $before})
    if($remaining.Count){throw ('Disposable child payload remains: '+($remaining -join ','))}
    $records+=@{Runner=$name;Mode=$mode;ExitCode=$child.ExitCode;PayloadRemoved=$true;SourceHash=(Get-FileHash -LiteralPath $original).Hash}
   } finally {$child.Dispose()}
  }
 }
 $records|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $root 'tmp\internal\verification\native_runner_retention.json') -Encoding UTF8
 Write-Host 'PASS six actual-runner failure/cancellation-exception paths; owned BIN payloads removed; no board or compiler execution.'
} finally {Remove-OwnedTestWorkspace $scope}
