# Exercise first-time setup against owned local bare remotes; never GitHub.
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
. (Join-Path $PSScriptRoot 'github_publish_catalog.ps1')
. (Join-Path $PSScriptRoot 'github_publish_initialize.ps1')
$scope=New-OwnedTestWorkspace 'publish_initialize' -ShortPath
$checks=New-Object 'Collections.Generic.List[string]'
$saved=@{}
foreach($key in @('GIT_AUTHOR_NAME','GIT_AUTHOR_EMAIL','GIT_COMMITTER_NAME','GIT_COMMITTER_EMAIL')) {
    $saved[$key]=[Environment]::GetEnvironmentVariable($key)
}
$oldCount=$env:GIT_CONFIG_COUNT; $oldKey=$env:GIT_CONFIG_KEY_0; $oldValue=$env:GIT_CONFIG_VALUE_0
$oldKey1=$env:GIT_CONFIG_KEY_1; $oldValue1=$env:GIT_CONFIG_VALUE_1
function Check($Value,[string]$Name) {
    if(!$Value) { throw $Name }; $checks.Add($Name); Write-Host "PASS: $Name"
}
function Refuses([scriptblock]$Action,[string]$Pattern) {
    try { & $Action } catch { if($_.Exception.Message -notmatch $Pattern) { throw }; return }
    throw "Missing expected refusal: $Pattern"
}
try {
    $env:GIT_AUTHOR_NAME='Publication fixture'; $env:GIT_COMMITTER_NAME=$env:GIT_AUTHOR_NAME
    $env:GIT_AUTHOR_EMAIL='fixture@example.invalid'; $env:GIT_COMMITTER_EMAIL=$env:GIT_AUTHOR_EMAIL
    $targets=@(Get-GithubPublishTargets '' -IncludeRh850 | Where-Object { $_.Name -like 'Sample_Project_RH850_*' })
    foreach($target in $targets) {
        $target.Root=Join-Path $scope.Path $target.Name
        $target.Remote=$target.Root+'.git'
        [void][IO.Directory]::CreateDirectory($target.Root)
        [IO.File]::WriteAllText((Join-Path $target.Root '.gitignore'),"tmp/`nDefaultBuild/*`n")
        [IO.File]::WriteAllText((Join-Path $target.Root 'source.c'),'/* fixture source */')
        foreach($group in $target.Groups) {
            [void][IO.Directory]::CreateDirectory((Join-Path $target.Root $group.Relative))
            foreach($keep in $group.Keep) { [IO.File]::WriteAllText((Join-Path $target.Root ($group.Relative+'/'+$keep)),'fixture image') }
        }
        [void](Invoke-PublishGit $scope.Path @('init','--bare',$target.Remote))
    }
    # Inherited test identity never changes the user's global configuration.
    $env:GIT_CONFIG_COUNT='2'; $env:GIT_CONFIG_KEY_0='user.name'; $env:GIT_CONFIG_VALUE_0='Publication fixture'
    $env:GIT_CONFIG_KEY_1='user.email'; $env:GIT_CONFIG_VALUE_1='fixture@example.invalid'
    $missing=@(Get-UninitializedPublishTargets $targets)
    Check ($missing.Count -eq 6 -and !(Test-Path -LiteralPath (Join-Path $targets[0].Root '.git'))) 'read-only plan finds six projects without initializing Git'
    $delivery=Join-Path $targets[-1].Root ($targets[-1].Groups[0].Relative+'/'+$targets[-1].Groups[0].Keep[0])
    $text=[IO.File]::ReadAllText($delivery); [IO.File]::WriteAllText($delivery,'')
    Refuses { Get-UninitializedPublishTargets $targets } 'Missing/empty delivery'
    Check (!(Test-Path -LiteralPath (Join-Path $targets[0].Root '.git'))) 'missing last delivery refuses before first initialization'
    [IO.File]::WriteAllText($delivery,$text)
    Refuses { Get-UninitializedPublishTargets @($targets[0],$targets[0]) } 'Duplicate/nested'
    $seed=Join-Path $scope.Path 'nonempty'; [void][IO.Directory]::CreateDirectory($seed)
    [void](Invoke-PublishGit $seed @('init','-b','main'))
    [void](Invoke-PublishGit $seed @('commit','--allow-empty','-m','fixture existing history'))
    [void](Invoke-PublishGit $seed @('push',$targets[-1].Remote,'main'))
    Refuses { Initialize-PublishTargets $missing } 'empty remote'
    Check (!(Test-Path -LiteralPath (Join-Path $targets[0].Root '.git'))) 'nonempty last remote refuses before first initialization'
    $targets[-1].Remote=Join-Path $scope.Path 'replacement-empty.git'
    [void](Invoke-PublishGit $scope.Path @('init','--bare',$targets[-1].Remote))
    Initialize-PublishTargets $missing
    foreach($target in $targets) {
        Check ((Invoke-PublishGit $target.Root @('ls-tree','--name-only','HEAD')).Text -ceq '') ('initial commit contains no firmware: '+$target.Name)
        Check ((Invoke-PublishGit $target.Root @('ls-remote','origin')).Text -ceq '') ('remote remains empty: '+$target.Name)
    }
    Check (@(Get-UninitializedPublishTargets $targets).Count -eq 0) 'repeat leaves initialized repositories intact'
} finally {
    foreach($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key]) }
    $env:GIT_CONFIG_COUNT=$oldCount; $env:GIT_CONFIG_KEY_0=$oldKey; $env:GIT_CONFIG_VALUE_0=$oldValue
    $env:GIT_CONFIG_KEY_1=$oldKey1; $env:GIT_CONFIG_VALUE_1=$oldValue1
    $bytes=(Get-ChildItem -LiteralPath $scope.Path -Recurse -Force -File | Measure-Object Length -Sum).Sum
    Remove-OwnedTestWorkspace $scope
    $record=@{Checks=$checks.ToArray();FixtureRemoved=!(Test-Path -LiteralPath $scope.Path);RetiredBytes=$bytes;Github=$false}
    $path=Join-Path (Split-Path -Parent $PSScriptRoot) 'tmp\internal\verification\publish_initialize_result.json'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $path))
    $record|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $path -Encoding UTF8
}
