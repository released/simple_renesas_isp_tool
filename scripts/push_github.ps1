[CmdletBinding()]
param([switch]$Execute,[switch]$Yes,[switch]$CanOnly,[switch]$IncludeRh850,
      [switch]$InitializeMissing,[switch]$CreateRemote,
      [string]$Message='Sync ISP tool, F24 host and CAN device examples',
      [string]$AcceptanceFile='',
      [string]$F13TransferProject='', [string]$F13BootProject='', [string]$F13AppProject='',
      [string]$F24TransferProject='', [string]$F24BootProject='', [string]$F24AppProject='',
      [string]$F24FdTransferProject='', [string]$F24FdBootProject='', [string]$F24FdAppProject='',
      [string]$Rh850TransferProject='', [string]$Rh850BootProject='', [string]$Rh850AppProject='',
      [string]$Rh850FdTransferProject='', [string]$Rh850FdBootProject='', [string]$Rh850FdAppProject='')
$ErrorActionPreference='Stop'
# Noninteractive confirmation cannot stand in for current-version board acceptance.
if($Execute -and $Yes -and !$AcceptanceFile) {
    throw 'Current board acceptance is required. -Yes cannot bypass it. Run interactively or provide a matching -AcceptanceFile. No paths, files, index, commits or remotes were changed.'
}
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
. (Join-Path $PSScriptRoot 'github_publish_catalog.ps1')
. (Join-Path $PSScriptRoot 'github_publish_release.ps1')
. (Join-Path $PSScriptRoot 'github_publish_initialize.ps1')
$toolRoot=Split-Path -Parent $PSScriptRoot
$local=Join-Path $toolRoot 'github_push.local.psd1'
if(Test-Path -LiteralPath $local) {
    $paths=Import-PowerShellDataFile -LiteralPath $local
    if(!$F13TransferProject) { $F13TransferProject=$paths.F13TransferProject }
    if(!$F13BootProject) { $F13BootProject=$paths.F13BootProject }
    if(!$F13AppProject) { $F13AppProject=$paths.F13AppProject }
    if(!$F24TransferProject) { $F24TransferProject=$paths.F24TransferProject }
    if(!$F24BootProject) { $F24BootProject=$paths.F24BootProject }
    if(!$F24AppProject) { $F24AppProject=$paths.F24AppProject }
    if(!$F24FdTransferProject) { $F24FdTransferProject=$paths.F24FdTransferProject }
    if(!$F24FdBootProject) { $F24FdBootProject=$paths.F24FdBootProject }
    if(!$F24FdAppProject) { $F24FdAppProject=$paths.F24FdAppProject }
    foreach($kind in @('Rh850Transfer','Rh850Boot','Rh850App','Rh850FdTransfer','Rh850FdBoot','Rh850FdApp')) {
        $key=$kind+'Project'
        if(!(Get-Variable -Name $key -ValueOnly) -and $paths.ContainsKey($key)) { Set-Variable -Name $key -Value $paths[$key] }
    }
}
$F13TransferProject=Get-CompanionPath -Kind F13Transfer -ExplicitPath $F13TransferProject
$F13BootProject=Get-CompanionPath -Kind F13Boot -ExplicitPath $F13BootProject
$F13AppProject=Get-CompanionPath -Kind F13App -ExplicitPath $F13AppProject
$F24TransferProject=Get-CompanionPath -Kind F24Transfer -ExplicitPath $F24TransferProject
$F24BootProject=Get-CompanionPath -Kind F24Boot -ExplicitPath $F24BootProject
$F24AppProject=Get-CompanionPath -Kind F24App -ExplicitPath $F24AppProject
$F24FdTransferProject=Get-CompanionPath -Kind F24FdTransfer -ExplicitPath $F24FdTransferProject
$F24FdBootProject=Get-CompanionPath -Kind F24FdBoot -ExplicitPath $F24FdBootProject
$F24FdAppProject=Get-CompanionPath -Kind F24FdApp -ExplicitPath $F24FdAppProject
$rh850Paths=@{}
if($IncludeRh850) {
    foreach($kind in @('Rh850Transfer','Rh850Boot','Rh850App','Rh850FdTransfer','Rh850FdBoot','Rh850FdApp')) {
        $key=$kind+'Project'
        $rh850Paths[$key]=Get-CompanionPath -Kind $kind -ExplicitPath (Get-Variable -Name $key -ValueOnly)
    }
}
$includeTool=!$CanOnly
$targets=@(Get-GithubPublishTargets $toolRoot $F13BootProject $F13AppProject `
    $F24TransferProject $F24BootProject $F24AppProject `
    $F24FdTransferProject $F24FdBootProject $F24FdAppProject $F13TransferProject -IncludeRh850:$IncludeRh850 @rh850Paths |
    Where-Object { $includeTool -or $_.Name -ne 'simple_renesas_isp_tool' })
$transcript=$false
$publicationStarted=$false
$initializationStarted=$false
try {
    Assert-CanPublishScope $targets -IncludeTool:$includeTool
    if($CreateRemote -and !$InitializeMissing) { throw '-CreateRemote requires -InitializeMissing.' }
    if($InitializeMissing) {
        $missing=@(Get-UninitializedPublishTargets $targets)
        if($missing.Count -and $Execute) {
            if($Yes) { throw 'First-time repository initialization requires interactive CREATE confirmation.' }
            Write-Host 'First-time repositories:'
            $missing | ForEach-Object { Write-Host ("  {0} -> {1}" -f $_.Root,$_.Remote) }
            if((Read-Host 'Initialize these repositories and optional public remotes? Type CREATE') -cne 'CREATE') {
                Write-Host 'Canceled without changes.'; exit 0
            }
            $initializationStarted=$true
            Initialize-PublishTargets $missing -CreateRemote:$CreateRemote
        } elseif($missing.Count) {
            $missing | ForEach-Object { Write-Host ("PLAN: initialize {0} -> {1}" -f $_.Root,$_.Remote) }
            Write-Host 'PLAN ONLY: no files/index/commits/remotes changed.'; exit 0
        }
    }
    $targets=New-PublishPlan $targets
    Assert-CanPublishScope $targets -IncludeTool:$includeTool
    [void](Invoke-PublishGit $toolRoot @('check-ignore','--no-index','tmp/__publish_probe__'))
    [void](Invoke-PublishGit $toolRoot @('check-ignore','--no-index','github_push.local.psd1'))
    Write-Host ("Publish main: {0} CAN device repositories, then Tool/F24 Host when selected." -f @($targets | Where-Object Name -ne 'simple_renesas_isp_tool').Count)
    if($CanOnly) { Write-Host 'CAN-only mode: Tool and F24 host are excluded.' }
    Write-Host 'Excluded: unselected firmware/support assets, serial MCU projects, runtime data and websites.'
    foreach($target in $targets) {
        Write-Host "`n$($target.Name) -> $($target.Remote) : $($target.Root)"
        Write-Host $target.Status
        if($target.DeferredPaths.Count) {
            Write-Host 'DEFERRED: these hardware-pending MCU project paths remain local and will not be staged/published:' -ForegroundColor Yellow
            $target.DeferredPaths | ForEach-Object { Write-Host ('  '+$_) }
        }
        foreach($group in $target.Plan) {
            Write-Host ("Keep {0}: {1}; archive {2} other entries" -f $group.Relative,($group.Keep.Name -join ', '),$group.Move.Count)
        }
    }
    $snapshot=Get-CanPublishSnapshot $targets -IncludeTool:$includeTool
    foreach($project in $snapshot.Projects) { Write-Host ("Reviewed inventory: {0}: {1} files" -f $project.Name,$project.Files.Count) }
    Write-Host "Release snapshot SHA256: $($snapshot.SnapshotId)"
    if(!$Execute) { Write-Host 'PLAN ONLY: no files/index/commits/remotes changed.'; exit 0 }
    if([string]::IsNullOrWhiteSpace($Message)) { throw 'Commit message is required.' }
    if($includeTool) {
        $review=Join-Path $toolRoot 'tmp\internal\github_publish_review.json'
        Assert-PublishTree $toolRoot $review
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $review))
        $snapshot | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $review -Encoding UTF8
        Write-Host "Exact file/hash inventory for review: $review"
    }
    $receiptName='last_can_acceptance.json'
    if($includeTool) { $receiptName='last_isp_acceptance.json' }
    if($IncludeRh850) { $receiptName='last_can15_acceptance.json'; if($includeTool) { $receiptName='last_isp16_acceptance.json' } }
    $cachedAcceptance=Join-Path $toolRoot ('tmp\internal\github_publish\'+$receiptName)
    $selectedAcceptance=$AcceptanceFile
    if(!$selectedAcceptance -and (Test-Path -LiteralPath $cachedAcceptance)) {
        Assert-PublishTree $toolRoot $cachedAcceptance
        try {
            $cached=Get-Content -LiteralPath $cachedAcceptance -Raw -Encoding UTF8 | ConvertFrom-Json
            Assert-CanReleaseAcceptance $cached $snapshot
            $selectedAcceptance=$cachedAcceptance
            Write-Host 'The recorded acceptance matches this exact source/image snapshot.'
        } catch {
            Write-Host 'The previous acceptance does not cover this snapshot; current-version confirmation is required.' -ForegroundColor Yellow
        }
    }
    $acceptance=Confirm-CanRelease $snapshot $selectedAcceptance
    if($selectedAcceptance -and !$Yes -and (Read-Host "Archive other DefaultBuild files, commit and push all $($targets.Count)? Type PUSH") -cne 'PUSH') {
        Write-Host 'Canceled without changes.'; exit 0
    }
    $backup=Join-Path $toolRoot ('tmp\internal\github_publish\'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'_'+[guid]::NewGuid().ToString('N'))
    Assert-PublishTree $toolRoot $backup
    [void](New-Item -ItemType Directory -Path $backup -Force)
    [void](Start-Transcript -Path (Join-Path $backup 'publish.log'))
    $transcript=$true
    Write-Host "Local recovery archive/log: $backup"
    $acceptance | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $backup 'release_acceptance.json') -Encoding UTF8
    $releaseGuard={ Assert-CanReleaseAcceptance $acceptance (Get-CanPublishSnapshot $targets -IncludeTool:$includeTool) }
    $publicationStarted=$true
    Invoke-GithubPublication $targets $backup $Message -ArchiveOwnerRoot $toolRoot -ReleaseGuard $releaseGuard
    Assert-PublishTree $toolRoot $cachedAcceptance
    $acceptance | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $cachedAcceptance -Encoding UTF8
    Write-Host "All $($targets.Count) remote main branches verified. No force push, build or website publication."
} catch {
    Write-Host "STOPPED: $($_.Exception.Message)" -ForegroundColor Red
    if($publicationStarted) {
        Write-Host 'Earlier moves/commits/pushes are not automatically rolled back. Review the log/backup and Git status.'
    } elseif($initializationStarted) {
        Write-Host 'First-time Git/remotes may already be initialized. No delivery outputs were archived; review initialization messages before retrying.'
    } else {
        Write-Host 'Stopped before publication: no delivery files were archived and no Git index, commits or remotes were changed.'
    }
    exit 1
} finally { if($transcript) { [void](Stop-Transcript) } }
