# Safe integration tests: disposable local worktrees/bare remotes only. No GitHub access.
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
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
    $targets=@(
        @{Name='RL78_F13_Boot_loader_CAN';Groups=@(@{Relative='DefaultBuild';Archive='f13_boot';Keep=@('boot0000_1FFF.hex')})},
        @{Name='RL78_F13_Boot_loader_CAN_app';Groups=@(@{Relative='DefaultBuild';Archive='f13_app';Keep=@('RL78_F13_Boot_loader_CAN_app_update.bin')})},
        @{Name='simple_renesas_isp_tool';Groups=@(
            @{Relative='firmware\RL78_F24_Bridge_boot\DefaultBuild';Archive='bridge_boot';Keep=@('RL78_F24_Bridge_boot.mot')},
            @{Relative='firmware\RL78_F24_Bridge_app\DefaultBuild';Archive='bridge_app';Keep=@('RL78_F24_Bridge_app_bridge_update.bin','RL78_F24_Bridge_app_flash.mot')})})
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
    }
    @{Root=$case;Targets=$targets;Backup=(Join-Path $targets[2].Root 'tmp\internal\github_publish\test-archive')}
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
Pass 'five delivery files, tracked output removal, tmp exclusion, backup, quoted arguments and verified three-repo push'
$heads=@($plan | ForEach-Object {$_.Head})
$plan=New-PublishPlan $case.Targets
Invoke-GithubPublication $plan (Join-Path $case.Targets[2].Root 'tmp\internal\github_publish\second-archive') 'no-op'
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
Assert-Test ($actual[1] -ceq $old[1] -and $actual[2] -ceq $old[2]) 'continued after rejection'
Assert-Test (Test-Path -LiteralPath (Join-Path $case.Backup 'f13_boot\diagnostic.map')) 'backup lost on push failure'
Pass 'partial push stops, retains local commits/backup, never force-pushes remaining repos'
Write-Host "$passed integration checks PASS. Evidence retained: $testRoot"
