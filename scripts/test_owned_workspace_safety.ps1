$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'workspace_guard'
$peer=New-OwnedTestWorkspace 'workspace_peer'
$link=Join-Path $scope.Path 'escape'
try {
    $sentinel=Join-Path $scope.Path 'keep.txt';[IO.File]::WriteAllText($sentinel,'owned test sentinel')
    $peerFile=Join-Path $peer.Path 'peer.txt';[IO.File]::WriteAllText($peerFile,'peer must remain')
    [IO.File]::SetAttributes($peerFile,[IO.FileAttributes]::ReadOnly)
    $rejected=0
    try{New-OwnedTestWorkspace '..\escape'|Out-Null}catch{$rejected++}
    try{Remove-OwnedTestWorkspace ([pscustomobject]@{Root=$scope.Root;Path=$scope.Path;Nonce='forged'})}catch{$rejected++}
    try{Remove-OwnedTestWorkspace ([pscustomobject]@{Root='C:\Users';Path='C:\Users\tmp\internal\tests\fake_00000000000000000000000000000000';Nonce=$scope.Nonce})}catch{$rejected++}
    New-Item -ItemType Junction -Path $link -Target $peer.Path | Out-Null
    try{Remove-OwnedTestWorkspace $scope}catch{$rejected++}
    if($rejected -ne 4 -or !(Test-Path -LiteralPath $sentinel) -or !(Test-Path -LiteralPath $peerFile)){throw 'Test cleanup preflight failed to preserve both sentinels.'}
    $junction=Get-Item -LiteralPath $link -Force
    if($junction.LinkType -ne 'Junction' -or [string]$junction.Target -cne $peer.Path){throw 'Unexpected owned junction.'}
    [IO.Directory]::Delete($link,$false)
    Remove-OwnedTestWorkspace $scope
    try{Remove-OwnedTestWorkspace $scope;throw 'Repeated cleanup was accepted.'}catch{
        if($_.Exception.Message -notmatch 'Unregistered'){throw}
    }
    Write-Host 'PASS fresh scope,nonce,immutable project boundary,junction preflight,zero partial delete and repeated cleanup rejection.'
} finally {
    if(Test-Path -LiteralPath $link){[IO.Directory]::Delete($link,$false)}
    if(Test-Path -LiteralPath $scope.Path){Remove-OwnedTestWorkspace $scope}
    Remove-OwnedTestWorkspace $peer
}
