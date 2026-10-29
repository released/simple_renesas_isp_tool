# Production entry/helpers exercised with synthetic local Git repositories only.
param([switch]$GateOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
. (Join-Path $PSScriptRoot 'github_publish_catalog.ps1')
. (Join-Path $PSScriptRoot 'github_publish_release.ps1')
$scope=New-OwnedTestWorkspace 'can_publish' -ShortPath
$results=New-Object 'Collections.Generic.List[string]'
$toolRoot=Split-Path -Parent $PSScriptRoot
function Check($Value,[string]$Name) {
    if(!$Value) { throw $Name }
    $results.Add($Name)
    Write-Host "PASS: $Name"
}
function Write-TestFile([string]$Path,[string]$Content) {
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    [IO.File]::WriteAllText($Path,$Content,(New-Object Text.UTF8Encoding($false)))
}
function Expect-Refusal([scriptblock]$Action,[string]$Pattern,[string]$Name) {
    try { & $Action } catch {
        if($_.Exception.Message -notmatch $Pattern) { throw }
        Check $true $Name
        return
    }
    throw "Missing refusal: $Name"
}
function Run-Entry([string[]]$Arguments,[int]$ExpectedExit,[string]$Pattern,[string]$InputText='') {
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\powershell.exe'
    $mode=@('-NoProfile')
    if(!$InputText) { $mode+='-NonInteractive' }
    $info.Arguments=($mode+@('-ExecutionPolicy','Bypass','-File',
        (Join-Path $fixtureTool 'scripts\push_github.ps1'))+$Arguments |
        ForEach-Object {ConvertTo-PublishArgument $_}) -join ' '
    $info.UseShellExecute=$false
    $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true
    $info.RedirectStandardError=$true
    $info.RedirectStandardInput=[bool]$InputText
    $process=New-Object Diagnostics.Process
    $process.StartInfo=$info
    try {
        [void]$process.Start()
        if($InputText) { $process.StandardInput.WriteLine($InputText); $process.StandardInput.Close() }
        $stdout=$process.StandardOutput.ReadToEndAsync()
        $stderr=$process.StandardError.ReadToEndAsync()
        if(!$process.WaitForExit(120000)) { $process.Kill(); throw 'Owned publisher fixture timed out.' }
        $output=$stdout.Result+"`n"+$stderr.Result
        if($process.ExitCode -ne $ExpectedExit -or $output -notmatch $Pattern) {
            throw "Entry result $($process.ExitCode), expected $ExpectedExit / $Pattern : $output"
        }
        return $output
    } finally { $process.Dispose() }
}
try {
    foreach($name in @('push_github.ps1','github_publish_release.ps1','github_publish_reset.ps1')) {
        $tokens=$null; $errors=$null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name),[ref]$tokens,[ref]$errors)
        if($errors.Count) { throw "Parse errors: $name" }
    }
    $fixtureTool=Join-Path $scope.Path 'simple_renesas_isp_tool'
    [void][IO.Directory]::CreateDirectory((Join-Path $fixtureTool 'scripts'))
    foreach($name in @('push_github.ps1','workspace_paths.ps1','github_publish_git.ps1',
        'github_publish_reset.ps1','github_publish_release.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $fixtureTool ('scripts\'+$name))
    }
    $targets=@(Get-GithubPublishTargets $fixtureTool)
    # Use the production catalog; only fixture roots/remotes are substituted.
    foreach($target in $targets) {
        $target.Root=Join-Path $scope.Path $target.Name
        $target.Remote=Join-Path $scope.Path ($target.Name+'.git')
    }
    $targets=@($targets | Where-Object Name -ne 'simple_renesas_isp_tool')
    Assert-CanPublishScope $targets
    $catalog=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'github_publish_catalog.ps1') -Raw -Encoding UTF8
    $catalog=$catalog.Replace('function Get-GithubPublishTargets','function Get-FixtureOriginalCatalog')
    $catalog+=@'

function Get-GithubPublishTargets {
    $all=@(Get-FixtureOriginalCatalog @args)
    foreach($target in $all) {
        $target.Root=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) $target.Name
        $target.Remote=$target.Root+'.git'
    }
    return $all
}
'@
    Write-TestFile (Join-Path $fixtureTool 'scripts\github_publish_catalog.ps1') $catalog
    foreach($target in $targets) {
        [void][IO.Directory]::CreateDirectory($target.Root)
        [void](Invoke-PublishGit $scope.Path @('init','--bare',$target.Remote))
        [void](Invoke-PublishGit $target.Root @('init','-b','main'))
        [void](Invoke-PublishGit $target.Root @('config','user.name','Synthetic CAN Publisher'))
        [void](Invoke-PublishGit $target.Root @('config','user.email','fixture@example.invalid'))
        Write-TestFile (Join-Path $target.Root '.gitignore') "tmp/`n"
        Write-TestFile (Join-Path $target.Root 'source.c') "/* synthetic source */`n"
        foreach($group in $target.Groups) {
            foreach($keep in $group.Keep) { Write-TestFile (Join-Path $target.Root ($group.Relative+'\'+$keep)) ('synthetic '+$keep) }
            Write-TestFile (Join-Path $target.Root ($group.Relative+'\diagnostic.map')) 'synthetic compiler output'
        }
        [void](Invoke-PublishGit $target.Root @('add','--all'))
        [void](Invoke-PublishGit $target.Root @('commit','-m','synthetic seed'))
        [void](Invoke-PublishGit $target.Root @('remote','add','origin',$target.Remote))
        [void](Invoke-PublishGit $target.Root @('push','origin','main'))
        Write-TestFile (Join-Path $target.Root 'source.c') "/* reviewed synthetic source */`n"
        Write-TestFile (Join-Path $target.Root 'tmp\private.log') 'preserve locally'
    }
    [void](Invoke-PublishGit $fixtureTool @('init','-b','main'))
    Write-TestFile (Join-Path $fixtureTool '.gitignore') "tmp/`ngithub_push.local.psd1`n"
    Write-TestFile (Join-Path $fixtureTool 'firmware\unvalidated\source.c') 'do not publish'
    Write-TestFile (Join-Path $fixtureTool 'build\runtime.ini') 'preserve runtime settings'
    $toolIndex=Join-Path $fixtureTool '.git\index'
    [void](Invoke-PublishGit $fixtureTool @('add','--all'))
    $protected=(Get-FileHash -LiteralPath $toolIndex).Hash
    $plan=New-PublishPlan $targets
    $snapshot=Get-CanPublishSnapshot $plan
    $acceptance=$snapshot | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    Expect-Refusal {Assert-CanReleaseAcceptance $acceptance $snapshot} 'no matching acceptance' 'unvalidated current snapshot refused'
    $acceptance.BoardValidated=$true
    $acceptance.Evidence='Synthetic fixture only, not hardware evidence.'
    Assert-CanReleaseAcceptance $acceptance $snapshot
    Check $true 'current-board receipt matches exact source/image inventory'
    $acceptance.ValidationBasis='prior-board-offline-regression'
    $acceptance.BoardValidated=$false
    Expect-Refusal {Assert-CanReleaseAcceptance $acceptance $snapshot} 'no matching acceptance' 'prior board evidence without explicit exception refused'
    $acceptance.ExceptionAuthorized=$true
    Assert-CanReleaseAcceptance $acceptance $snapshot
    Check $true 'explicit snapshot-bound exception retains BoardValidated false'
    Expect-Refusal {Get-CanPublishSnapshot @($plan[0..7])} 'exactly the nine' 'incomplete release scope refused'
    Expect-Refusal {Get-CanPublishSnapshot @($plan+@{Name='simple_renesas_isp_tool'})} 'exactly the nine' 'Tool or extra firmware target refused'
    $receipt=Join-Path $fixtureTool 'tmp\acceptance.json'
    Write-TestFile $receipt ($acceptance | ConvertTo-Json -Depth 10)
    $before=@($plan | ForEach-Object {(Get-FileHash -LiteralPath (Join-Path $_.Root '.git\index')).Hash})
    [void](Run-Entry @('-Execute','-Yes') 1 'board acceptance is required')
    Check $true '-Yes alone refused before dependency/path/network actions'
    [void](Run-Entry @('-Execute') 1 'Read-Host|NonInteractive|not confirmed')
    Check $true 'noninteractive execution cannot silently confirm board tests'
    $source=Join-Path $plan[0].Root 'source.c'
    Write-TestFile $source 'changed after review, identical Git short status'
    [void](Run-Entry @('-Execute','-Yes','-AcceptanceFile',$receipt) 1 'no matching acceptance')
    Write-TestFile $source "/* reviewed synthetic source */`n"
    Check $true 'source content drift refused before archiving/index/commit/push'
    $delivery=Join-Path $plan[0].Root ('DefaultBuild\'+$plan[0].Groups[0].Keep[0])
    $original=[IO.File]::ReadAllBytes($delivery)
    Write-TestFile $delivery 'changed synthetic image'
    Expect-Refusal {Assert-CanReleaseAcceptance $acceptance (Get-CanPublishSnapshot $plan)} 'no matching acceptance' 'delivery hash drift refused'
    [IO.File]::WriteAllBytes($delivery,$original)
    for($i=0;$i -lt $plan.Count;$i++) {
        Check ((Get-FileHash -LiteralPath (Join-Path $plan[$i].Root '.git\index')).Hash -ceq $before[$i]) ('refusal preserves index '+$plan[$i].Name)
    }
    Check (!(Test-Path -LiteralPath (Join-Path $fixtureTool 'tmp\internal\github_publish'))) 'refusals create no archive'
    if(!$GateOnly) {
        [void](Invoke-PublishGit $plan[0].Root @('tag','-a','fixture-local-only','-m','do not push'))
        [void](Invoke-PublishGit $plan[0].Root @('config','push.followTags','true'))
        [void](Run-Entry @('-Execute','-Yes','-AcceptanceFile',$receipt,'-Message','Reviewed "CAN" examples') 0 'All 9 remote main branches verified')
        foreach($target in $plan) {
            $head=(Invoke-PublishGit $target.Root @('rev-parse','HEAD')).Text
            $remote=(Invoke-PublishGit $target.Root @('ls-remote','origin','refs/heads/main')).Text.Split("`t")[0]
            if($head -cne $remote) { throw 'Unverified fixture remote.' }
            if((Invoke-PublishGit $target.Root @('status','--porcelain')).Text) { throw 'Fixture has remaining source changes.' }
            if((Get-ChildItem -LiteralPath (Join-Path $target.Root 'DefaultBuild')).Count -ne 1) { throw 'Wrong build output count.' }
            if(!(Test-Path -LiteralPath (Join-Path $target.Root 'tmp\private.log'))) { throw 'Private fixture log removed.' }
        }
        Check $true 'actual entry commits/pushes exactly nine repositories and nine delivery files'
        Check ((Get-FileHash -LiteralPath $toolIndex).Hash -ceq $protected) 'Tool index and unvalidated firmware untouched; no Tool EXE required'
        Check (!(Invoke-PublishGit $plan[0].Root @('ls-remote','origin','refs/tags/*')).Text) 'local tags remain unpublished'
        $archives=@(Get-ChildItem -LiteralPath (Join-Path $fixtureTool 'tmp\internal\github_publish') -Directory)
        Check (@(Get-ChildItem -LiteralPath $archives[0].FullName -Filter diagnostic.map -Recurse -File).Count -eq 9) 'all nine compiler outputs recoverably archived'
        $heads=@($plan | ForEach-Object {(Invoke-PublishGit $_.Root @('rev-parse','HEAD')).Text})
        [void](Run-Entry @('-Execute') 0 'The recorded acceptance matches' 'PUSH')
        Check (@(Compare-Object $heads @($plan | ForEach-Object {(Invoke-PublishGit $_.Root @('rev-parse','HEAD')).Text})).Count -eq 0) 'repeat publication makes no empty commits'
        $plan=New-PublishPlan $targets
        Write-TestFile (Join-Path $plan[0].Root 'DefaultBuild\diagnostic.map') 'new synthetic output'
        $plan=New-PublishPlan $targets
        $guard={Write-TestFile $source 'drift before first move'; Assert-CanReleaseAcceptance $acceptance (Get-CanPublishSnapshot $plan)}
        Expect-Refusal {Invoke-GithubPublication $plan (Join-Path $fixtureTool 'tmp\internal\github_publish\race') 'do not publish' -ArchiveOwnerRoot $fixtureTool -ReleaseGuard $guard} 'no matching acceptance' 'post-fetch source drift refused before first move'
        Check (Test-Path -LiteralPath (Join-Path $plan[0].Root 'DefaultBuild\diagnostic.map')) 'race refusal preserves compiler output'
        Write-TestFile $source "/* reviewed synthetic source */`n"
        Expect-Refusal {Invoke-GithubPublication $plan (Join-Path $scope.Path 'outside-archive') 'do not publish' -ArchiveOwnerRoot $fixtureTool} 'escapes' 'archive outside owner boundary refused'
    }
} finally {
    Remove-OwnedTestWorkspace $scope
    $report=[ordered]@{Cases=$results.Count;Passed=$results.ToArray();FixtureRemoved=!(Test-Path -LiteralPath $scope.Path)
        HardwareRun=$false;GithubPush=$false;GateOnly=[bool]$GateOnly;Utc=[DateTime]::UtcNow.ToString('o')}
    $reportPath=Join-Path $toolRoot 'tmp\internal\verification\can_publication_result.json'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $reportPath))
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding UTF8
}
Write-Host "$($results.Count) scoped CAN publication checks PASS; owned fixture retired."
