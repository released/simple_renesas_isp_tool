# A release decision belongs to one exact selected source/image inventory.
function Assert-CanPublishScope($Targets,[switch]$IncludeTool) {
    $expected=@('RL78_F13_Boot_loader_CAN','RL78_F13_Boot_loader_CAN_app','RL78_F13_CAN_Transfer',
        'RL78_F24_CAN_Transfer','RL78_F24_Boot_loader_CAN','RL78_F24_Boot_loader_CAN_app',
        'RL78_F24_CAN_FD_Transfer','RL78_F24_Boot_loader_CAN_FD','RL78_F24_Boot_loader_CAN_FD_app')
    if(@($Targets | Where-Object { $_.Name -like 'Sample_Project_RH850_*' }).Count) {
        $expected+=@('Sample_Project_RH850_S1_CAN_Transfer','Sample_Project_RH850_S1_Boot_loader_CAN',
            'Sample_Project_RH850_S1_Boot_loader_CAN_app','Sample_Project_RH850_S1_CAN_FD_Transfer',
            'Sample_Project_RH850_S1_Boot_loader_CAN_FD','Sample_Project_RH850_S1_Boot_loader_CAN_FD_app')
    }
    if($IncludeTool) { $expected+='simple_renesas_isp_tool' }
    if(@($Targets | Where-Object { $_.Name -in @('RL78_F24_I2C_Slave_Transfer','RL78_F24_ISP_Boot_loader_I2C','RL78_F24_ISP_Boot_loader_I2C_app') }).Count) {
        $expected+=@('RL78_F24_I2C_Slave_Transfer','RL78_F24_ISP_Boot_loader_I2C','RL78_F24_ISP_Boot_loader_I2C_app')
    }
    if($Targets.Count -ne $expected.Count -or
       @(Compare-Object ($Targets.Name | Sort-Object) ($expected | Sort-Object)).Count) {
        throw 'Release scope requires nine RL78 CAN projects, optional complete RH850 CAN and F24 I2C triplets, and optional Tool. Partial or serial firmware outside the selected F24 I2C triplet is not accepted.'
    }
}

function Get-CanPublishSnapshot($Targets,[switch]$IncludeTool) {
    Assert-CanPublishScope $Targets -IncludeTool:$IncludeTool
    $projects=@()
    foreach($target in ($Targets | Sort-Object { [string]$_.Name })) {
        $keepers=@($target.Plan | ForEach-Object {
            $relative=$_.Relative.Replace('\','/')
            $_.Keep | ForEach-Object { $relative+'/'+$_.Name }
        })
        $paths=@((Invoke-PublishGit $target.Root @('ls-files','--cached','--others','--exclude-standard','-z')).Text.Split([char]0))
        $deferred=@(Get-PublishDeferredPaths $target)
        $files=@()
        foreach($path in (@($paths+$keepers+@($target.RequiredFiles)) | Where-Object {$_} | Sort-Object -Unique)) {
            if($path -cin $deferred) { continue }
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
        $project=[ordered]@{Name=$target.Name;Root=$target.Root;Remote=$target.Remote;Files=$files}
        if($target.ToolScope) { $project.Dependencies=@(Get-PublishToolDependencies $target) }
        $projects+=$project
    }
    $json=ConvertTo-Json -InputObject $projects -Depth 8 -Compress
    $sha=[Security.Cryptography.SHA256]::Create()
    try {
        $id=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($json))).Replace('-','')
    } finally { $sha.Dispose() }
    $schema=1; $scope='rl78-can-golden-9'
    if($IncludeTool) { $schema=2; $scope='renesas-isp-tool-host-can-10' }
    if(@($Targets | Where-Object { $_.Name -like 'Sample_Project_RH850_*' }).Count) {
        $schema=3; $scope='renesas-can-golden-15'
        if($IncludeTool) { $scope='renesas-isp-tool-host-can-16' }
    }
    if(@($Targets | Where-Object { $_.Name -eq 'RL78_F24_I2C_Slave_Transfer' }).Count) {
        $schema=4
        $scope='renesas-rl78-can-f24-i2c-12'
        if($IncludeTool) { $scope='renesas-tool-host-rl78-can-f24-i2c-13' }
        if(@($Targets | Where-Object { $_.Name -like 'Sample_Project_RH850_*' }).Count) {
            $scope='renesas-can-f24-i2c-18'
            if($IncludeTool) { $scope='renesas-isp-tool-host-can-f24-i2c-19' }
        }
    }
    [pscustomobject][ordered]@{Schema=$schema;Scope=$scope;SnapshotId=$id
        CreatedUtc=[DateTime]::UtcNow.ToString('o');BoardValidated=$false
        ValidationBasis='current-board';ExceptionAuthorized=$false;Evidence='';Projects=$projects}
}

function Assert-CanReleaseAcceptance($Acceptance,$Snapshot) {
    $basis=($Acceptance.ValidationBasis -ceq 'current-board' -and $Acceptance.BoardValidated -eq $true) -or
        ($Acceptance.ValidationBasis -ceq 'prior-board-offline-regression' -and
         $Acceptance.BoardValidated -eq $false -and $Acceptance.ExceptionAuthorized -is [bool] -and
         $Acceptance.ExceptionAuthorized -eq $true)
    if($Acceptance.Schema -ne $Snapshot.Schema -or $Acceptance.Scope -cne $Snapshot.Scope -or
       $Acceptance.BoardValidated -isnot [bool] -or !$basis -or
       [string]::IsNullOrWhiteSpace($Acceptance.Evidence) -or
       $Acceptance.SnapshotId -cne $Snapshot.SnapshotId) {
        throw 'Current release has no matching acceptance. Confirm current-board validation or an explicitly authorized prior-board/offline-regression exception for the displayed source/image inventory. An older acceptance or -Yes cannot authorize this snapshot.'
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
    Write-Host "Publish scope: $($Snapshot.Projects.Count) repositories. Review every selected file in the displayed inventory; withheld MCU files and websites remain excluded."
    Write-Host 'Confirm actual-board tests of the CURRENT source/images, not just an older build or offline tests.' -ForegroundColor Yellow
    try {
        $confirmation=Read-Host 'If all selected CURRENT MCU firmware passed real-board validation and this exact inventory should be published, type VALIDATED_AND_PUSH'
    } catch {
        throw 'NonInteractive publication requires a matching -AcceptanceFile; board validation was not confirmed.'
    }
    if($confirmation -cne 'VALIDATED_AND_PUSH') {
        throw 'Current-version board acceptance was not confirmed. No delivery files/index/commits/remotes changed.'
    }
    $Snapshot.BoardValidated=$true
    $Snapshot.Evidence='Maintainer explicitly confirmed real-board validation and publication of the displayed current source/image inventory at '+[DateTime]::UtcNow.ToString('o')
    return $Snapshot
}
