[CmdletBinding()]
param([switch]$Execute,[switch]$Yes,[string]$Message='Publish Tool and MCU CAN device projects',
      [string]$F13BootProject='', [string]$F13AppProject='',
      [string]$F24TransferProject='', [string]$F24BootProject='', [string]$F24AppProject='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
. (Join-Path $PSScriptRoot 'github_publish_catalog.ps1')
$toolRoot=Split-Path -Parent $PSScriptRoot
$local=Join-Path $toolRoot 'github_push.local.psd1'
if(Test-Path -LiteralPath $local) {
    $paths=Import-PowerShellDataFile -LiteralPath $local
    if(!$F13BootProject) { $F13BootProject=$paths.F13BootProject }
    if(!$F13AppProject) { $F13AppProject=$paths.F13AppProject }
    if(!$F24TransferProject) { $F24TransferProject=$paths.F24TransferProject }
    if(!$F24BootProject) { $F24BootProject=$paths.F24BootProject }
    if(!$F24AppProject) { $F24AppProject=$paths.F24AppProject }
}
$F13BootProject=Get-CompanionPath -Kind F13Boot -ExplicitPath $F13BootProject
$F13AppProject=Get-CompanionPath -Kind F13App -ExplicitPath $F13AppProject
$F24TransferProject=Get-CompanionPath -Kind F24Transfer -ExplicitPath $F24TransferProject
$F24BootProject=Get-CompanionPath -Kind F24Boot -ExplicitPath $F24BootProject
$F24AppProject=Get-CompanionPath -Kind F24App -ExplicitPath $F24AppProject
$targets=@(Get-GithubPublishTargets $toolRoot $F13BootProject $F13AppProject `
    $F24TransferProject $F24BootProject $F24AppProject)
$transcript=$false
try {
    $targets=New-PublishPlan $targets
    $exe=Get-Item -LiteralPath (Join-Path $toolRoot 'build\FirmwareUpgradeTool.exe')
    if(!$exe.Length) { throw 'Tool delivery EXE is empty.' }
    [void](Invoke-PublishGit $toolRoot @('check-ignore','--no-index','tmp/__publish_probe__'))
    [void](Invoke-PublishGit $toolRoot @('check-ignore','--no-index','github_push.local.psd1'))
    Write-Host 'Publish main: F13 Boot -> F13 App -> F24 device Transfer -> Boot -> App -> Tool (including F24 CAN host).'
    Write-Host 'All unignored working-tree changes in all six repositories are included.'
    foreach($target in $targets) {
        Write-Host "`n$($target.Name) -> $($target.Remote) : $($target.Root)"
        Write-Host $target.Status
        foreach($group in $target.Plan) {
            Write-Host ("Keep {0}: {1}; archive {2} other entries" -f $group.Relative,($group.Keep.Name -join ', '),$group.Move.Count)
        }
    }
    if(!$Execute) { Write-Host 'PLAN ONLY: no files/index/commits/remotes changed.'; exit 0 }
    if([string]::IsNullOrWhiteSpace($Message)) { throw 'Commit message is required.' }
    if(!$Yes -and (Read-Host 'Archive other DefaultBuild files, commit and push all six? Type PUSH') -cne 'PUSH') {
        Write-Host 'Canceled without changes.'; exit 0
    }
    $backup=Join-Path $toolRoot ('tmp\internal\github_publish\'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'_'+[guid]::NewGuid().ToString('N'))
    Assert-PublishTree $toolRoot $backup
    [void](New-Item -ItemType Directory -Path $backup -Force)
    [void](Start-Transcript -Path (Join-Path $backup 'publish.log'))
    $transcript=$true
    Write-Host "Local recovery archive/log: $backup"
    Invoke-GithubPublication $targets $backup $Message
    Write-Host 'All six remote main branches verified. No force push, build or website publication.'
} catch {
    Write-Host "STOPPED: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Earlier moves/commits/pushes are not automatically rolled back. Review the log/backup and Git status.'
    exit 1
} finally { if($transcript) { [void](Stop-Transcript) } }
