# Disposable native-test workspaces stay inside the selected Tool checkout.
function New-OwnedTestWorkspace([string]$Label,[switch]$ShortPath) {
    if($Label -notmatch '^[A-Za-z0-9_]+$'){throw 'Invalid test workspace label.'}
    $root=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
    $id=[guid]::NewGuid().ToString('N')
    if($ShortPath){$id=$id.Substring(0,8)}
    $path=Join-Path $root ('tmp\internal\tests\'+$Label+'_'+$id)
    $probe=$path
    while($probe.Length -ge $root.Length){
        if((Test-Path -LiteralPath $probe) -and ((Get-Item -LiteralPath $probe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Reparse test workspace refused.'}
        $probe=Split-Path -Parent $probe
    }
    if(Test-Path -LiteralPath $path){throw 'Test workspace must be fresh.'}
    [IO.Directory]::CreateDirectory($path)|Out-Null
    if(!(Get-Variable -Name SimpleIspOwnedTestWorkspaces -Scope Global -ErrorAction SilentlyContinue)){$global:SimpleIspOwnedTestWorkspaces=@{}}
    $nonce=[guid]::NewGuid().ToString('N')
    $global:SimpleIspOwnedTestWorkspaces[$path]=$nonce
    $scope=[pscustomobject]@{Root=$root;Path=$path;Nonce=$nonce}
    [IO.File]::WriteAllText((Join-Path $path '.owned-test-workspace.json'),($scope|ConvertTo-Json))
    return $scope
}
function Remove-OwnedTestWorkspace($Scope) {
    $root=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
    $path=[IO.Path]::GetFullPath($Scope.Path).TrimEnd('\')
    $prefix=Join-Path $root 'tmp\internal\tests'
    if($Scope.Root -cne $root -or !$path.StartsWith($prefix+'\',[StringComparison]::OrdinalIgnoreCase) -or
       [IO.Path]::GetFileName($path) -notmatch '_(?:[0-9a-f]{8}|[0-9a-f]{32})$'){throw 'Test cleanup boundary mismatch.'}
    if(!(Get-Variable -Name SimpleIspOwnedTestWorkspaces -Scope Global -ErrorAction SilentlyContinue) -or
       !$global:SimpleIspOwnedTestWorkspaces.ContainsKey($path) -or
       $global:SimpleIspOwnedTestWorkspaces[$path] -cne $Scope.Nonce){throw 'Unregistered test workspace refused.'}
    $directories=New-Object 'Collections.Generic.List[string]'
    $files=New-Object 'Collections.Generic.List[string]'
    $queue=New-Object 'Collections.Generic.Queue[string]';$queue.Enqueue($path)
    while($queue.Count){
        $folder=$queue.Dequeue()
        if((Get-Item -LiteralPath $folder -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse test cleanup refused.'}
        $directories.Add($folder)
        foreach($item in Get-ChildItem -LiteralPath $folder -Force){
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse test child refused.'}
            if($item.PSIsContainer){$queue.Enqueue($item.FullName)}else{$files.Add($item.FullName)}
        }
    }
    foreach($file in $files){
        $attributes=[IO.File]::GetAttributes($file)
        if($attributes -band [IO.FileAttributes]::ReadOnly){[IO.File]::SetAttributes($file,($attributes -band (-bnot [IO.FileAttributes]::ReadOnly)))}
    }
    foreach($file in $files){[IO.File]::Delete($file)}
    for($i=$directories.Count-1;$i -ge 0;$i--){[IO.Directory]::Delete($directories[$i],$false)}
    $global:SimpleIspOwnedTestWorkspaces.Remove($path)
}
