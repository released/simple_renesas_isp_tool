# Git process helpers: no shell evaluation, force push, reset, or automatic merge.
function ConvertTo-PublishArgument([string]$Value) {
    '"'+[regex]::Replace([regex]::Replace($Value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"'
}

function Invoke-PublishGit {
    param([string]$Root, [string[]]$Arguments, [int[]]$Allowed=@(0))
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=(Get-Command git -ErrorAction Stop).Source
    $info.Arguments=(@('-C',$Root,'-c','core.quotepath=false')+$Arguments |
        ForEach-Object { ConvertTo-PublishArgument $_ }) -join ' '
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    $info.StandardOutputEncoding=New-Object Text.UTF8Encoding($false)
    $info.StandardErrorEncoding=New-Object Text.UTF8Encoding($false)
    $info.EnvironmentVariables['GIT_TERMINAL_PROMPT']='0'
    $info.EnvironmentVariables['GCM_INTERACTIVE']='Never'
    $info.EnvironmentVariables['GIT_OPTIONAL_LOCKS']='0'
    $process=New-Object Diagnostics.Process
    $process.StartInfo=$info
    try {
        [void]$process.Start()
        $output=$process.StandardOutput.ReadToEndAsync()
        $errors=$process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $result=[pscustomobject]@{Code=$process.ExitCode;Text=$output.Result.TrimEnd();Error=$errors.Result.TrimEnd()}
        if($result.Code -notin $Allowed) {
            throw ('git {0} failed ({1}): {2} {3}' -f ($Arguments -join ' '),$result.Code,$result.Text,$result.Error)
        }
        return $result
    } finally { $process.Dispose() }
}

function Assert-PublishRemote([string]$Value, [string]$Expected) {
    if($Expected -match '^released/[A-Za-z0-9_]+$') {
        $escaped=[regex]::Escape($Expected)
        if($Value -notmatch ('^(https://github\.com/|git@github\.com:|ssh://git@github\.com/)'+$escaped+'(?:\.git)?/?$')) {
            throw "Origin does not match the expected GitHub repository: $Expected"
        }
    } elseif($Value -cne $Expected) { throw 'Origin does not match the explicit test repository.' }
}

function Assert-PublishRepository($Target) {
    $root=(Resolve-Path -LiteralPath $Target.Root -ErrorAction Stop).ProviderPath.TrimEnd('\','/')
    if((Split-Path -Leaf $root) -cne $Target.Name) { throw "Unexpected repository directory: $root" }
    Assert-PublishTree $root $root
    $actual=(Invoke-PublishGit $root @('rev-parse','--show-toplevel')).Text.Replace('/','\').TrimEnd('\')
    if($actual -ine $root.Replace('/','\')) { throw "Not an exact repository root: $root" }
    if((Invoke-PublishGit $root @('symbolic-ref','--short','HEAD')).Text -cne 'main') {
        throw "Publication requires main (no detached HEAD): $root"
    }
    foreach($gitArguments in @(@('remote','get-url','--all','origin'),@('remote','get-url','--push','--all','origin'))) {
        Assert-PublishRemote (Invoke-PublishGit $root $gitArguments).Text $Target.Remote
    }
    if((Invoke-PublishGit $root @('config','--bool','--get','remote.origin.mirror') @(0,1)).Text -eq 'true') {
        throw "Mirror push configuration is not supported: $root"
    }
    foreach($name in @('index.lock','MERGE_HEAD','CHERRY_PICK_HEAD','REVERT_HEAD','rebase-merge','rebase-apply')) {
        $path=(Invoke-PublishGit $root @('rev-parse','--git-path',$name)).Text
        if(![IO.Path]::IsPathRooted($path)) { $path=Join-Path $root $path }
        if(Test-Path -LiteralPath $path) { throw "Finish the active Git operation first: $name in $root" }
    }
    if((Invoke-PublishGit $root @('diff','--cached','--quiet') @(0,1)).Code) {
        throw "Existing staged changes must be reviewed/committed before batch publication: $root"
    }
    foreach($key in @('user.name','user.email')) {
        if(!(Invoke-PublishGit $root @('config','--get',$key) @(0,1)).Text) { throw "Missing Git $key in $root" }
    }
    $Target.Root=$root
    $Target.Head=(Invoke-PublishGit $root @('rev-parse','HEAD')).Text
    $Target.Status=(Invoke-PublishGit $root @('status','--short','--untracked-files=all')).Text
}

function Assert-PublishAncestry($Targets) {
    foreach($target in $Targets) {
        [void](Invoke-PublishGit $target.Root @('fetch','--no-tags','origin','refs/heads/main'))
        if((Invoke-PublishGit $target.Root @('merge-base','--is-ancestor','FETCH_HEAD','HEAD') @(0,1)).Code) {
            throw "Remote main contains changes not in local main. Review/synchronize before publishing: $($target.Name)"
        }
    }
}
