# A release decision belongs to one exact nine-project CAN source/image snapshot.
function Assert-CanPublishScope($Targets) {
    $expected=@('RL78_F13_Boot_loader_CAN','RL78_F13_Boot_loader_CAN_app','RL78_F13_CAN_Transfer',
        'RL78_F24_CAN_Transfer','RL78_F24_Boot_loader_CAN','RL78_F24_Boot_loader_CAN_app',
        'RL78_F24_CAN_FD_Transfer','RL78_F24_Boot_loader_CAN_FD','RL78_F24_Boot_loader_CAN_FD_app')
    if($Targets.Count -ne $expected.Count -or
       @(Compare-Object ($Targets.Name | Sort-Object) ($expected | Sort-Object)).Count) {
        throw 'Release scope must contain exactly the nine F13/F24 Classical CAN and F24 CAN FD device repositories.'
    }
}

function Get-CanPublishSnapshot($Targets) {
    Assert-CanPublishScope $Targets
    $projects=@()
    foreach($target in ($Targets | Sort-Object { [string]$_.Name })) {
        $keepers=@($target.Plan | ForEach-Object {
            $relative=$_.Relative.Replace('\','/')
            $_.Keep | ForEach-Object { $relative+'/'+$_.Name }
        })
        $paths=@((Invoke-PublishGit $target.Root @('ls-files','--cached','--others','--exclude-standard','-z')).Text.Split([char]0))
        $files=@()
        foreach($path in (@($paths+$keepers) | Where-Object {$_} | Sort-Object -Unique)) {
            $inBuild=$false
            foreach($group in $target.Plan) {
                if($path.StartsWith($group.Relative.Replace('\','/')+'/',[StringComparison]::OrdinalIgnoreCase)) {
                    $inBuild=$true
                }
            }
            if($inBuild -and $path -cnotin $keepers) { continue }
            if(Test-PublishLocalOnlyPath $path) { continue }
            if($path -match '[\r\n\t]') { throw 'Control characters are not supported in publication file names.' }
            $full=Join-Path $target.Root $path
            Assert-PublishTree $target.Root $full
            if(!(Test-Path -LiteralPath $full -PathType Leaf)) { continue }
            $files+= [ordered]@{Path=$path;Sha256=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash}
        }
        $projects+= [ordered]@{Name=$target.Name;Root=$target.Root;Remote=$target.Remote;Files=$files}
    }
    $json=ConvertTo-Json -InputObject $projects -Depth 8 -Compress
    $sha=[Security.Cryptography.SHA256]::Create()
    try {
        $id=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($json))).Replace('-','')
    } finally { $sha.Dispose() }
    [pscustomobject][ordered]@{Schema=1;Scope='rl78-can-golden-9';SnapshotId=$id
        CreatedUtc=[DateTime]::UtcNow.ToString('o');BoardValidated=$false
        ValidationBasis='current-board';ExceptionAuthorized=$false;Evidence='';Projects=$projects}
}

function Assert-CanReleaseAcceptance($Acceptance,$Snapshot) {
    $basis=($Acceptance.ValidationBasis -ceq 'current-board' -and $Acceptance.BoardValidated -eq $true) -or
        ($Acceptance.ValidationBasis -ceq 'prior-board-offline-regression' -and
         $Acceptance.BoardValidated -eq $false -and $Acceptance.ExceptionAuthorized -is [bool] -and
         $Acceptance.ExceptionAuthorized -eq $true)
    if($Acceptance.Schema -ne 1 -or $Acceptance.Scope -cne 'rl78-can-golden-9' -or
       $Acceptance.BoardValidated -isnot [bool] -or !$basis -or
       [string]::IsNullOrWhiteSpace($Acceptance.Evidence) -or
       $Acceptance.SnapshotId -cne $Snapshot.SnapshotId) {
        throw 'Current CAN release has no matching acceptance. Confirm current-board validation or an explicitly authorized prior-board/offline-regression exception for these nine source/image versions. An older acceptance or -Yes cannot authorize this snapshot.'
    }
    $accepted=ConvertTo-Json -InputObject @($Acceptance.Projects) -Depth 8 -Compress
    $current=ConvertTo-Json -InputObject @($Snapshot.Projects) -Depth 8 -Compress
    if($accepted -cne $current) { throw 'Accepted project/file inventory differs from the current release snapshot.' }
}

function Confirm-CanRelease($Snapshot,[string]$AcceptanceFile) {
    if($AcceptanceFile) {
        $record=Get-Content -LiteralPath $AcceptanceFile -Raw -Encoding UTF8 | ConvertFrom-Json
        Assert-CanReleaseAcceptance $record $Snapshot
        return $record
    }
    Write-Host 'Only these nine device CAN repositories will be published. Tool/Host, RH850, UART/I2C/Xmodem and websites are excluded.'
    Write-Host 'Confirm actual-board tests of the CURRENT source/images, not just an older build or offline tests.' -ForegroundColor Yellow
    try {
        $confirmation=Read-Host 'If all nine CURRENT versions passed real-board validation and should be pushed, type VALIDATED_AND_PUSH'
    } catch {
        throw 'NonInteractive publication requires a matching -AcceptanceFile; board validation was not confirmed.'
    }
    if($confirmation -cne 'VALIDATED_AND_PUSH') {
        throw 'Current-version board acceptance was not confirmed. No delivery files/index/commits/remotes changed.'
    }
    $Snapshot.BoardValidated=$true
    $Snapshot.Evidence='Maintainer explicitly confirmed real-board validation and publication of the displayed current nine-project source/image snapshot at '+[DateTime]::UtcNow.ToString('o')
    return $Snapshot
}
