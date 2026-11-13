# Standalone F13/F24 I2C publisher; all helpers are local, no builds or device access.
param([switch]$DryRun,[switch]$Yes,[string]$AcceptanceFile='',
      [string]$Message='Update validated RL78 I2C example')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
. (Join-Path $PSScriptRoot 'github_publish_release.ps1')
$root=Split-Path -Parent $PSScriptRoot
$name=Split-Path -Leaf $root
$deliveries=@{
    RL78_F13_I2C_Slave_Transfer='RL78_F13_I2C_Slave_Transfer.hex'
    RL78_F13_ISP_Boot_loader_I2C='boot0000_1FFF.hex'
    RL78_F13_ISP_Boot_loader_I2C_app='RL78_F13_ISP_Boot_loader_I2C_app_update.bin'
    RL78_F24_I2C_Slave_Transfer='RL78_F24_I2C_Slave_Transfer.hex'
    RL78_F24_ISP_Boot_loader_I2C='boot0000_1FFF.hex'
    RL78_F24_ISP_Boot_loader_I2C_app='RL78_F24_ISP_Boot_loader_I2C_app_update.bin'
}
try {
    if(!$deliveries.ContainsKey($name)){throw 'Only the selected F13/F24 I2C project roots are supported.'}
    if([string]::IsNullOrWhiteSpace($Message)){throw 'Commit message is required.'}
    $gh=Get-Command gh -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1 -ExpandProperty Source
    if(!$gh -and (Test-Path -LiteralPath (Join-Path $env:ProgramFiles 'GitHub CLI/gh.exe'))){$gh=Join-Path $env:ProgramFiles 'GitHub CLI/gh.exe'}
    if(!$gh){throw 'Installed GitHub CLI is required to verify the live repository identity.'}
    $identity=& $gh api ('repos/released/'+$name)
    if($LASTEXITCODE){throw 'Live repository identity could not be verified.'}
    $identity=$identity|ConvertFrom-Json
    if($identity.full_name -cne ('released/'+$name) -or $identity.private -or $identity.archived -or $identity.default_branch -cne 'main'){
        throw 'Expected active PUBLIC released/project main repository.'
    }
    $target=@{Name=$name;Root=$root;Remote=('released/'+$name)
        Groups=@(@{Relative='DefaultBuild';Archive='device';Keep=@($deliveries[$name])})}
    $plan=New-PublishPlan @($target)
    $keepers=@($target.Plan|ForEach-Object {$_.Keep|ForEach-Object {'DefaultBuild/'+$_.Name}})
    $paths=(Invoke-PublishGit $root @('ls-files','--cached','--others','--exclude-standard','-z')).Text.Split([char]0)
    $files=@()
    foreach($path in (@($paths)+$keepers|Where-Object {$_}|Sort-Object -Unique)){
        if(Test-PublishLocalOnlyPath $path){continue}
        if($path.StartsWith('DefaultBuild/') -and $path -cnotin $keepers){continue}
        $full=Join-Path $root $path
        Assert-PublishTree $root $full
        if(Test-Path -LiteralPath $full -PathType Leaf){$files+=[ordered]@{Path=$path;Sha256=(Get-FileHash -LiteralPath $full).Hash}}
    }
    $projects=@([ordered]@{Name=$name;Root=$root;Remote=$target.Remote;Files=$files})
    $sha=[Security.Cryptography.SHA256]::Create()
    try{$id=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes((ConvertTo-Json -InputObject $projects -Depth 8 -Compress)))).Replace('-','')}
    finally{$sha.Dispose()}
    $family=if($name.StartsWith('RL78_F13_')){'f13'}else{'f24'}
    $snapshot=[pscustomobject]@{Schema=4;Scope=($family+'-i2c-single/'+$name);SnapshotId=$id;Projects=$projects
        BoardValidated=$false;ValidationBasis='current-board';ExceptionAuthorized=$false;Evidence=''}
    Write-Host ($target.Remote+' / main / PUBLIC; '+$files.Count+' selected files; snapshot '+$id)
    Write-Host $target.Status
    if($DryRun){Write-Host 'PLAN ONLY: no archive, staging, commit, push or repository creation.';exit 0}
    $cache=Join-Path $root 'tmp/internal/github_publish/accepted.json'
    $selected=$AcceptanceFile
    if(!$selected -and (Test-Path -LiteralPath $cache)){
        try{$candidate=Get-Content -LiteralPath $cache -Raw -Encoding UTF8|ConvertFrom-Json;Assert-CanReleaseAcceptance $candidate $snapshot;$selected=$cache}
        catch{Write-Host 'Previous board acceptance does not cover the current snapshot.'}
    }
    if(!$selected -and $Yes){throw '-Yes requires a matching current-version -AcceptanceFile; no mutation.'}
    $acceptance=Confirm-CanRelease $snapshot $selected
    if($selected -and !$Yes -and (Read-Host 'Commit/push the reviewed current RL78 I2C snapshot? Type PUSH') -cne 'PUSH'){
        Write-Host 'Canceled without changes.';exit 0
    }
    $backup=Join-Path $root ('tmp/internal/github_publish/'+[guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($backup)
    $guard={foreach($file in $files){if((Get-FileHash -LiteralPath (Join-Path $root $file.Path)).Hash -cne $file.Sha256){throw 'Reviewed source changed.'}}}
    & $guard
    Invoke-GithubPublication $plan $backup $Message -ArchiveOwnerRoot $root -ReleaseGuard $guard
    $acceptance|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $cache -Encoding UTF8
    Write-Host 'Remote main verified. No build, device operation or website publication.'
}catch{Write-Error $_.Exception.Message -ErrorAction Continue;exit 1}
