# Explicit first-time setup; normal publication and plan-only never call this.
function Get-UninitializedPublishTargets($Targets) {
    $missing=@()
    $roots=@()
    foreach($target in $Targets) {
        $root=(Resolve-Path -LiteralPath $target.Root -ErrorAction Stop).ProviderPath.TrimEnd('\','/')
        if((Split-Path -Leaf $root) -cne $target.Name) { throw "Unexpected repository directory: $root" }
        Assert-PublishTree $root $root
        foreach($other in $roots) {
            if($root -ieq $other -or $root.StartsWith($other+'\',[StringComparison]::OrdinalIgnoreCase) -or
               $other.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Duplicate/nested publication roots are not supported.' }
        }
        $roots+=$root
        $target.Root=$root
        if(Test-Path -LiteralPath (Join-Path $root '.git')) {
            Assert-PublishRepository $target
        } else {
            if(!(Test-Path -LiteralPath (Join-Path $root '.gitignore') -PathType Leaf)) {
                throw "First-time publication requires a reviewed .gitignore: $root"
            }
            foreach($key in @('user.name','user.email')) {
                if(!(Invoke-PublishGit $root @('config','--get',$key) @(0,1)).Text) { throw "Missing Git $key in $root" }
            }
            $missing+=$target
        }
        # Check every delivery and reparse boundary before creating any Git data.
        [void]@(Get-PublishGroups $target)
    }
    return $missing
}

function Invoke-PublishGh([string]$Executable,[string[]]$Arguments,[int[]]$Allowed=@(0)) {
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=$Executable
    $start.Arguments=($Arguments | ForEach-Object { ConvertTo-PublishArgument $_ }) -join ' '
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $process=New-Object Diagnostics.Process
    $process.StartInfo=$start
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $result=[pscustomobject]@{Code=$process.ExitCode;Text=$stdout.Result.TrimEnd();Error=$stderr.Result.TrimEnd()}
        if($result.Code -notin $Allowed) { throw "GitHub initialization failed: $($result.Error)" }
        return $result
    } finally { $process.Dispose() }
}

function Initialize-PublishTargets($Targets,[switch]$CreateRemote) {
    $gh=$null
    if($CreateRemote) {
        $command=Get-Command gh -CommandType Application -ErrorAction SilentlyContinue
        if($command) { $gh=$command.Source }
        elseif(Test-Path -LiteralPath (Join-Path $env:ProgramFiles 'GitHub CLI\gh.exe')) {
            $gh=Join-Path $env:ProgramFiles 'GitHub CLI\gh.exe'
        }
        if(!$gh) { throw 'GitHub CLI is required for -CreateRemote; no repository was initialized.' }
        [void](Invoke-PublishGh $gh @('auth','status'))
    }
    $plans=@()
    foreach($target in $Targets) {
        $url=$target.Remote
        $create=$false
        if($target.Remote -match '^released/[A-Za-z0-9_]+$') {
            $url='https://github.com/'+$target.Remote+'.git'
            if($CreateRemote) {
                $view=Invoke-PublishGh $gh @('api',('repos/'+$target.Remote)) @(0,1)
                if($view.Code) {
                    if($view.Error -notmatch 'HTTP 404') { throw "Cannot establish remote state: $($target.Remote): $($view.Error)" }
                    $create=$true
                }
            }
        } elseif($CreateRemote) { throw 'Remote creation accepts only the selected released/name catalog entries.' }
        if(!$create -and (Invoke-PublishGit $target.Root @('ls-remote',$url)).Text) {
            throw "First-time initialization requires an empty remote: $($target.Remote). Recover its existing Git history before publication."
        }
        $plans+=@{Target=$target;Url=$url;Create=$create}
    }
    foreach($plan in $plans) {
        $target=$plan.Target
        if($plan.Create) { [void](Invoke-PublishGh $gh @('repo','create',$target.Remote,'--public')) }
        [void](Invoke-PublishGit $target.Root @('init','-b','main'))
        [void](Invoke-PublishGit $target.Root @('remote','add','origin',$plan.Url))
        [void](Invoke-PublishGit $target.Root @('commit','--allow-empty','-m','Initialize MCU example repository'))
        Write-Host "INITIALIZED $($target.Name); no source files staged or pushed."
    }
}
