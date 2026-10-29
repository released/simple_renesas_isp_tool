param([Parameter(Mandatory=$true)][string]$ReferenceRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
. (Join-Path $PSScriptRoot 'device_project_catalog.ps1')
$scope=New-OwnedTestWorkspace 'hal_init' -ShortPath
$result=[ordered]@{Result='FAIL';Projects=0;NegativeCases=0;FixtureRemoved=$false}
try {
    $dest=Join-Path $scope.Path 'samples'
    & (Join-Path $PSScriptRoot 'initialize_device_samples.ps1') -DeviceRoot $dest -ReferenceRoot $ReferenceRoot -Bus @('CAN','CAN_FD','UART','I2C','Xmodem')
    & (Join-Path $PSScriptRoot 'verify_device_hal_layout.ps1') -DeviceRoot $dest
    $projects=@(Get-DeviceProjectCatalog $dest)
    if($projects.Count -ne 60){throw 'Expected sixty independent initialized samples.'}
    foreach($project in $projects) {
        foreach($folder in @('DefaultBuild','test_bins','.git','tmp')) {
            if(Test-Path -LiteralPath (Join-Path $project.Project $folder)){throw 'Template copied disposable/private outputs.'}
        }
    }
    $result.Projects=$projects.Count
    $missing=Join-Path $scope.Path 'missing_reference'
    $rejected=$false
    try{& (Join-Path $PSScriptRoot 'initialize_device_samples.ps1') -DeviceRoot $missing -Mcu F13 -Bus CAN}catch{
        if($_.Exception.Message -notmatch 'ReferenceRoot'){throw};$rejected=$true
    }
    if(!$rejected -or (Test-Path -LiteralPath $missing)){throw 'Missing reference must refuse before output.'}
    $result.NegativeCases++
    $blocked=Join-Path $scope.Path 'existing'
    $app=Join-Path $blocked 'RL78_F13_Boot_loader_CAN_app'
    [IO.Directory]::CreateDirectory($app)|Out-Null
    $sentinel=Join-Path $app 'keep.txt';[IO.File]::WriteAllText($sentinel,'keep')
    $rejected=$false
    try{& (Join-Path $PSScriptRoot 'initialize_device_samples.ps1') -DeviceRoot $blocked -ReferenceRoot $ReferenceRoot -Mcu F13 -Bus CAN}catch{
        if($_.Exception.Message -notmatch 'never overwritten'){throw};$rejected=$true
    }
    if(!$rejected -or (Test-Path -LiteralPath (Join-Path $blocked 'RL78_F13_Boot_loader_CAN')) -or
        [IO.File]::ReadAllText($sentinel) -cne 'keep'){throw 'Group preflight changed a pre-existing project.'}
    $result.NegativeCases++
    $source=Join-Path $dest 'RL78_F13_Boot_loader_CAN_app'
    $mtpj=Join-Path $source 'RL78_F13_Boot_loader_CAN_app.mtpj'
    [xml]$xml=[IO.File]::ReadAllText($mtpj)
    $xml.SelectSingleNode('//Instance[Type="File"]/RelativePath').InnerText='..\escape.c'
    $xml.Save($mtpj)
    $escape=Join-Path $scope.Path 'escaped'
    $rejected=$false
    try{& (Join-Path $PSScriptRoot 'initialize_device_samples.ps1') -DeviceRoot $escape -ReferenceRoot $dest -Mcu F13 -Bus CAN -Role app}catch{
        if($_.Exception.Message -notmatch 'Non-local project source'){throw};$rejected=$true
    }
    if(!$rejected -or (Test-Path -LiteralPath $escape)){throw 'Escaping donor input was accepted.'}
    $result.NegativeCases++
    $result.Result='PASS'
} finally {
    Remove-OwnedTestWorkspace $scope
    $result.FixtureRemoved=!(Test-Path -LiteralPath $scope.Path)
    $evidence=Join-Path (Split-Path -Parent $PSScriptRoot) 'tmp\internal\verification'
    [IO.Directory]::CreateDirectory($evidence)|Out-Null
    $result|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $evidence 'device_hal_initialization.json')
}
Write-Host ('PASS '+$result.Projects+' independent templates, '+$result.NegativeCases+' negative preflight cases; owned fixture removed.')
