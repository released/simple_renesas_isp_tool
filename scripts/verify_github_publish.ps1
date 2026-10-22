# Safe integration tests: disposable local worktrees/bare remotes only. No GitHub access.
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
. (Join-Path $PSScriptRoot 'github_publish_catalog.ps1')
$testRoot=Join-Path (Split-Path -Parent $PSScriptRoot) ('tmp\internal\github_publish_tests\'+[guid]::NewGuid().ToString('N').Substring(0,8))
[void](New-Item -ItemType Directory -Path $testRoot -Force)
$passed=0
function Assert-Test($Value,[string]$Message) { if(!$Value) { throw $Message } }
function Write-Fixture([string]$Path,[string]$Text) {
    [void](New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force)
    [IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false)))
}
function Expect-Stop([scriptblock]$Action,[string]$Pattern) {
    try { & $Action } catch {
        if($_.Exception.Message -notmatch $Pattern) { throw }
        return
    }
    throw "Expected refusal: $Pattern"
}
function New-TestCase([string]$Name) {
    $case=Join-Path $testRoot ($Name+' with spaces')
    [void](New-Item -ItemType Directory -Path $case -Force)
    $targets=@(Get-GithubPublishTargets (Join-Path $case 'simple_renesas_isp_tool') `
        (Join-Path $case 'RL78_F13_Boot_loader_CAN') (Join-Path $case 'RL78_F13_Boot_loader_CAN_app') `
        (Join-Path $case 'RL78_F24_CAN_Transfer') (Join-Path $case 'RL78_F24_Boot_loader_CAN') `
        (Join-Path $case 'RL78_F24_Boot_loader_CAN_app') `
        (Join-Path $case 'RL78_F24_CAN_FD_Transfer') (Join-Path $case 'RL78_F24_Boot_loader_CAN_FD') `
        (Join-Path $case 'RL78_F24_Boot_loader_CAN_FD_app') (Join-Path $case 'RL78_F13_CAN_Transfer'))
    Assert-Test ($targets.Count -eq 10) 'production catalog must contain ten repositories'
    Assert-Test (@($targets | ForEach-Object {$_.Groups | ForEach-Object {$_.Keep}}).Count -eq 12) 'production catalog must retain twelve deliveries'
    foreach($target in $targets) {
        $target.Root=Join-Path $case $target.Name
        $target.Remote=Join-Path $case ($target.Name+'.git')
        [void](New-Item -ItemType Directory -Path $target.Root -Force)
        [void](Invoke-PublishGit $case @('init','--bare',$target.Remote))
        [void](Invoke-PublishGit $target.Root @('init','-b','main'))
        [void](Invoke-PublishGit $target.Root @('config','user.name','Publication Test'))
        [void](Invoke-PublishGit $target.Root @('config','user.email','publication@example.invalid'))
        Write-Fixture (Join-Path $target.Root '.gitignore') "tmp/`ngithub_push.local.psd1`n"
        Write-Fixture (Join-Path $target.Root 'README.md') "initial`n"
        if($target.DeferredPattern) { Write-Fixture (Join-Path $target.Root 'docs\RH850_tracked.md') "initial RH850`n" }
        foreach($group in $target.Groups) {
            foreach($name in $group.Keep) { Write-Fixture (Join-Path $target.Root ($group.Relative+'\'+$name)) ('delivery '+$name) }
            # Start tracked to verify that pruning removes files from the remote index.
            Write-Fixture (Join-Path $target.Root ($group.Relative+'\diagnostic.map')) 'diagnostic'
            Write-Fixture (Join-Path $target.Root ($group.Relative+'\nested\history.txt')) 'history'
        }
        [void](Invoke-PublishGit $target.Root @('add','--all'))
        [void](Invoke-PublishGit $target.Root @('commit','-m','seed'))
        [void](Invoke-PublishGit $target.Root @('remote','add','origin',$target.Remote))
        [void](Invoke-PublishGit $target.Root @('push','origin','main'))
        Write-Fixture (Join-Path $target.Root 'README.md') "updated 'quoted' message`n"
        Write-Fixture (Join-Path $target.Root 'tmp\local.log') 'never publish'
        if($target.DeferredPattern) {
            Write-Fixture (Join-Path $target.Root 'docs\RH850_tracked.md') "local changed RH850`n"
            Write-Fixture (Join-Path $target.Root 'docs\RH850_NEW.md') 'local RH850 draft'
            Write-Fixture (Join-Path $target.Root 'scripts\build_rh850.ps1') 'local RH850 script'
            Write-Fixture (Join-Path $target.Root 'tests\rh850_mock\r_port.h') 'local RH850 mock'
        }
    }
    @{Root=$case;Targets=$targets;Backup=(Join-Path $targets[-1].Root 'tmp\internal\github_publish\test-archive')}
}
function Pass([string]$Name) { $script:passed++; Write-Host "PASS: $Name" }

$case=New-TestCase 'normal'
[void](Invoke-PublishGit $case.Targets[0].Root @('tag','-a','local-only-tag','-m','do not publish'))
[void](Invoke-PublishGit $case.Targets[0].Root @('config','push.followTags','true'))
$plan=New-PublishPlan $case.Targets
Invoke-GithubPublication $plan $case.Backup 'Test "quoted" commit message'
foreach($target in $plan) {
    Assert-Test (!(Invoke-PublishGit $target.Root @('ls-files','tmp/')).Text) 'tmp leaked'
    foreach($group in $target.Plan) {
        Assert-Test ((Get-ChildItem -LiteralPath $group.Folder).Count -eq $group.Keep.Count) 'wrong remaining files'
        Assert-Test (Test-Path -LiteralPath (Join-Path $case.Backup ($group.Archive+'\diagnostic.map'))) 'missing recovery archive'
    }
    Assert-Test ((Invoke-PublishGit $target.Root @('log','-1','--format=%s')).Text -ceq 'Test "quoted" commit message') 'argument quoting failed'
    Assert-Test ((Invoke-PublishGit $target.Root @('ls-remote','origin','refs/heads/main')).Text.Split("`t")[0] -ceq $target.Head) 'wrong remote head'
    Assert-Test (!(Invoke-PublishGit $target.Root @('ls-remote','origin','refs/tags/*')).Text) 'unrequested tag published'
}
Pass 'twelve delivery files, tracked output removal, tmp exclusion, backup, quoted arguments and verified ten-repo push'
$tool=$plan[-1]
Assert-Test ((Invoke-PublishGit $tool.Root @('show','HEAD:docs/RH850_tracked.md')).Text -ceq 'initial RH850') 'tracked RH850 change was published'
foreach($path in @('docs/RH850_NEW.md','scripts/build_rh850.ps1','tests/rh850_mock/r_port.h')) {
    Assert-Test (!(Invoke-PublishGit $tool.Root @('ls-tree','--name-only','HEAD','--',$path)).Text) 'new RH850 file was published'
    Assert-Test (Test-Path -LiteralPath (Join-Path $tool.Root $path)) 'RH850 local file was removed'
}
Assert-Test ([IO.File]::ReadAllText((Join-Path $tool.Root 'docs/RH850_tracked.md')) -ceq "local changed RH850`n") 'RH850 local draft changed'
Pass 'tracked and untracked RH850 development stays local with content preserved'
$heads=@($plan | ForEach-Object {$_.Head})
$plan=New-PublishPlan $case.Targets
Invoke-GithubPublication $plan (Join-Path $case.Targets[-1].Root 'tmp\internal\github_publish\second-archive') 'no-op'
Assert-Test ((@($plan | ForEach-Object {$_.Head}) -join ',') -ceq ($heads -join ',')) 'no-op created commits'
Pass 'idempotent rerun'

$case=New-TestCase 'missing'
$path=Join-Path $case.Targets[0].Root 'DefaultBuild\boot0000_1FFF.hex'
Assert-PublishTree $case.Root $path
Move-Item -LiteralPath $path -Destination (Join-Path $case.Root 'saved.hex')
Expect-Stop { New-PublishPlan $case.Targets } 'Cannot find|does not exist|Missing'
Assert-Test (Test-Path -LiteralPath (Join-Path $case.Targets[0].Root 'DefaultBuild\diagnostic.map')) 'cleanup ran on failed validation'
Pass 'missing keeper stops before pruning'

$case=New-TestCase 'staged'
[void](Invoke-PublishGit $case.Targets[0].Root @('add','README.md'))
Expect-Stop { New-PublishPlan $case.Targets } 'Existing staged'
Pass 'pre-existing index preserved'

$case=New-TestCase 'tracked-local-data'
$localFile=Join-Path $case.Targets[4].Root 'tmp\build\app_rebuild.log'
Write-Fixture $localFile 'retain this local log'
[void](Invoke-PublishGit $case.Targets[4].Root @('add','-f','--','tmp/build/app_rebuild.log'))
[void](Invoke-PublishGit $case.Targets[4].Root @('commit','-m','seed tracked local data'))
$heads=@($case.Targets | ForEach-Object {(Invoke-PublishGit $_.Root @('rev-parse','HEAD')).Text})
Expect-Stop { New-PublishPlan $case.Targets } 'Local-only data is tracked.*RL78_F24_Boot_loader_CAN.*app_rebuild'
foreach($target in $case.Targets) {
    Assert-Test (!(Invoke-PublishGit $target.Root @('diff','--cached','--name-only')).Text) 'index changed before local-only refusal'
    foreach($group in $target.Groups) {
        Assert-Test (Test-Path -LiteralPath (Join-Path $target.Root ($group.Relative+'\diagnostic.map'))) 'archived before local-only refusal'
    }
}
Assert-Test ((@($case.Targets | ForEach-Object {(Invoke-PublishGit $_.Root @('rev-parse','HEAD')).Text}) -join ',') -ceq ($heads -join ',')) 'commit before local-only refusal'
Pass 'tracked local-only data refuses before any archive, stage or commit'

$localHash=(Get-FileHash -LiteralPath $localFile).Hash
[void](Invoke-PublishGit $case.Targets[4].Root @('rm','--cached','--','tmp/build/app_rebuild.log'))
$plan=New-PublishPlan $case.Targets
Invoke-GithubPublication $plan $case.Backup 'remove local-data tracking'
Assert-Test ((Get-FileHash -LiteralPath $localFile).Hash -ceq $localHash) 'local file changed during tracking removal'
Assert-Test (!(Invoke-PublishGit $case.Targets[4].Root @('ls-tree','-r','--name-only','HEAD','--','tmp/')).Text) 'local data still committed'
Pass 'explicit staged local-only deletion can publish without deleting the local file'

$case=New-TestCase 'unignored-local-data'
Write-Fixture (Join-Path $case.Targets[4].Root '.gitignore') "github_push.local.psd1`n"
Expect-Stop { New-PublishPlan $case.Targets } 'Local-only data is not ignored'
Assert-Test (Test-Path -LiteralPath (Join-Path $case.Targets[4].Root 'DefaultBuild\diagnostic.map')) 'archived before ignore refusal'
Assert-Test (!(Invoke-PublishGit $case.Targets[4].Root @('diff','--cached','--name-only')).Text) 'index changed on ignore refusal'
Pass 'unignored local-only file refuses before cleanup or staging'

$case=New-TestCase 'wrong-remote'
$case.Targets[0].Remote='released/RL78_F13_Boot_loader_CAN'
Expect-Stop { New-PublishPlan $case.Targets } 'Origin does not match'
Pass 'wrong remote rejected'

$case=New-TestCase 'mirror'
[void](Invoke-PublishGit $case.Targets[0].Root @('config','remote.origin.mirror','yes'))
Expect-Stop { New-PublishPlan $case.Targets } 'Mirror push'
Pass 'mirror configuration rejected before cleanup'

$case=New-TestCase 'branch'
[void](Invoke-PublishGit $case.Targets[0].Root @('checkout','-b','other'))
Expect-Stop { New-PublishPlan $case.Targets } 'requires main'
Pass 'non-main branch rejected'

$case=New-TestCase 'junction'
$outside=Join-Path $case.Root 'outside'
Write-Fixture (Join-Path $outside 'sentinel.txt') 'preserve'
[void](New-Item -ItemType Junction -Path (Join-Path $case.Targets[0].Root 'DefaultBuild\escape') -Target $outside)
Expect-Stop { New-PublishPlan $case.Targets } 'Reparse points'
Assert-Test ([IO.File]::ReadAllText((Join-Path $outside 'sentinel.txt')) -ceq 'preserve') 'junction target touched'
Pass 'junction escape rejected without following target'

$case=New-TestCase 'remote-ahead'
$plan=New-PublishPlan $case.Targets
$other=Join-Path $case.Root 'another clone'
[void](Invoke-PublishGit $case.Root @('clone','--branch','main',$case.Targets[0].Remote,$other))
[void](Invoke-PublishGit $other @('config','user.name','Publication Test'))
[void](Invoke-PublishGit $other @('config','user.email','publication@example.invalid'))
Write-Fixture (Join-Path $other 'remote.txt') 'remote advance'
[void](Invoke-PublishGit $other @('add','remote.txt'))
[void](Invoke-PublishGit $other @('commit','-m','remote advance'))
[void](Invoke-PublishGit $other @('push','origin','main'))
Expect-Stop { Invoke-GithubPublication $plan $case.Backup 'must not commit' } 'Remote main contains'
Assert-Test (Test-Path -LiteralPath (Join-Path $case.Targets[0].Root 'DefaultBuild\diagnostic.map')) 'pruned before remote preflight'
Assert-Test ((Invoke-PublishGit $case.Targets[0].Root @('rev-parse','HEAD')).Text -ceq $plan[0].Head) 'commit occurred before remote preflight'
Pass 'remote advance refuses without merge, cleanup or commit'

$case=New-TestCase 'changed-after-plan'
$plan=New-PublishPlan $case.Targets
Write-Fixture (Join-Path $case.Targets[0].Root 'DefaultBuild\boot0000_1FFF.hex') 'changed after approval'
Expect-Stop { Invoke-GithubPublication $plan $case.Backup 'must not commit' } 'changed after approval'
Assert-Test (Test-Path -LiteralPath (Join-Path $case.Targets[0].Root 'DefaultBuild\diagnostic.map')) 'pruned after stale approval'
Pass 'changed approval rejected'

$case=New-TestCase 'push-failure'
$plan=New-PublishPlan $case.Targets
$old=@($plan | ForEach-Object {$_.Head})
Write-Fixture (Join-Path $case.Targets[1].Remote 'hooks\pre-receive') "#!/bin/sh`nexit 1`n"
Expect-Stop { Invoke-GithubPublication $plan $case.Backup 'partial push test' } 'failed'
$actual=@($plan | ForEach-Object {(Invoke-PublishGit $_.Root @('ls-remote','origin','refs/heads/main')).Text.Split("`t")[0]})
Assert-Test ($actual[0] -ceq $plan[0].Head -and $actual[0] -cne $old[0]) 'first successful push lost'
foreach($index in 1..($plan.Count-1)) { Assert-Test ($actual[$index] -ceq $old[$index]) 'continued after rejection' }
Assert-Test (Test-Path -LiteralPath (Join-Path $case.Backup 'f13_boot\diagnostic.map')) 'backup lost on push failure'
Pass 'partial push stops, retains local commits/backup, never force-pushes remaining repos'

$case=New-TestCase 'uninitialized-device'
$gitDirectory=Join-Path $case.Targets[2].Root '.git'
Assert-PublishTree $case.Root $gitDirectory -Recursive
Move-Item -LiteralPath $gitDirectory -Destination (Join-Path $case.Root 'saved-device-git')
Expect-Stop { New-PublishPlan $case.Targets } 'not initialized'
foreach($target in $case.Targets) {
    foreach($group in $target.Groups) {
        Assert-Test (Test-Path -LiteralPath (Join-Path $target.Root ($group.Relative+'\diagnostic.map'))) 'pruning occurred on uninitialized repo'
    }
}
Pass 'uninitialized device stops all ten before pruning or staging'

$case=New-TestCase 'uninitialized-fd-device'
$gitDirectory=Join-Path $case.Targets[5].Root '.git'
Assert-PublishTree $case.Root $gitDirectory -Recursive
Move-Item -LiteralPath $gitDirectory -Destination (Join-Path $case.Root 'saved-fd-device-git')
Expect-Stop { New-PublishPlan $case.Targets } 'not initialized'
foreach($target in $case.Targets) {
    foreach($group in $target.Groups) {
        Assert-Test (Test-Path -LiteralPath (Join-Path $target.Root ($group.Relative+'\diagnostic.map'))) 'pruning occurred on uninitialized FD repo'
    }
}
Pass 'uninitialized device stops all ten before pruning or staging'

foreach($index in 2..8) {
    $case=New-TestCase ('missing-device-'+$index)
    $path=Join-Path $case.Targets[$index].Root ('DefaultBuild\'+$case.Targets[$index].Groups[0].Keep[0])
    Assert-PublishTree $case.Root $path
    Move-Item -LiteralPath $path -Destination (Join-Path $case.Root 'saved-delivery')
    Expect-Stop { New-PublishPlan $case.Targets } 'Cannot find|does not exist|Missing'
    Assert-Test (Test-Path -LiteralPath (Join-Path $case.Targets[$index].Root 'DefaultBuild\diagnostic.map')) 'pruned on missing F24 delivery'
}
Pass 'each Classical and FD F24 delivery is mandatory before any cleanup'

$case=New-TestCase 'empty-device-origin'
$empty=Join-Path $case.Root 'initial-device.git'
[void](Invoke-PublishGit $case.Root @('init','--bare',$empty))
[void](Invoke-PublishGit $case.Targets[2].Root @('remote','set-url','origin',$empty))
$case.Targets[2].Remote=$empty
$plan=New-PublishPlan $case.Targets
Invoke-GithubPublication $plan $case.Backup 'first main publication'
Assert-Test ((Invoke-PublishGit $case.Targets[2].Root @('ls-remote','origin','refs/heads/main')).Text.Split("`t")[0] -ceq $plan[2].Head) 'initial main was not verified'
Pass 'verified empty origin accepts initial main without force push or repo creation'

$case=New-TestCase 'nonempty-without-main'
[void](Invoke-PublishGit $case.Targets[2].Root @('push','origin','HEAD:refs/heads/other'))
[void](Invoke-PublishGit $case.Root @('--git-dir',$case.Targets[2].Remote,'update-ref','-d','refs/heads/main'))
$plan=New-PublishPlan $case.Targets
Expect-Stop { Invoke-GithubPublication $plan $case.Backup 'must not publish' } 'not empty and has no main'
Assert-Test (Test-Path -LiteralPath (Join-Path $case.Targets[2].Root 'DefaultBuild\diagnostic.map')) 'pruned on ambiguous remote branch'
Pass 'nonempty origin lacking main refuses before cleanup'
Write-Host "$passed integration checks PASS. Evidence retained: $testRoot"
