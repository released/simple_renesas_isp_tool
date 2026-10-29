# Release-specific, recoverable DefaultBuild pruning. Never used by normal clean/build.
function Assert-PublishTree([string]$Root,[string]$Path,[switch]$Recursive) {
    $boundary=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    $full=[IO.Path]::GetFullPath($Path).TrimEnd('\','/')
    if($full -ine $boundary -and !$full.StartsWith($boundary+'\',[StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes its exact boundary: $full"
    }
    $current=$full
    while($true) {
        if(Test-Path -LiteralPath $current) {
            if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse points are not allowed in publication paths: $current"
            }
        }
        if($current -ieq $boundary) { break }
        $current=Split-Path -Parent $current
        if(!$current) { throw 'Unable to resolve a bounded publication path.' }
    }
    if($Recursive -and (Test-Path -LiteralPath $full -PathType Container)) {
        foreach($child in @(Get-ChildItem -LiteralPath $full -Force)) {
            Assert-PublishTree $boundary $child.FullName -Recursive
        }
    }
}

function Get-PublishGroups($Target) {
    foreach($group in $Target.Groups) {
        $folder=Join-Path $Target.Root $group.Relative
        Assert-PublishTree $Target.Root $folder -Recursive
        if((Split-Path -Leaf $folder) -cne 'DefaultBuild' -or !(Test-Path -LiteralPath $folder -PathType Container)) {
            throw "Expected exact DefaultBuild directory: $folder"
        }
        $keepers=@()
        foreach($name in $group.Keep) {
            if([IO.Path]::GetFileName($name) -cne $name) { throw 'Keeper must be a single file name.' }
            $path=Join-Path $folder $name
            $file=Get-Item -LiteralPath $path -ErrorAction Stop
            if($file.PSIsContainer -or !$file.Length) { throw "Missing/empty delivery: $path" }
            $keepers+=@{Name=$name;Hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
        }
        if(!$group.Archive -or [IO.Path]::GetFileName($group.Archive) -cne $group.Archive) { throw 'Archive label must be a single directory name.' }
        [pscustomobject]@{Folder=$folder;Relative=$group.Relative;Archive=$group.Archive;Keep=$keepers
            Move=@(Get-ChildItem -LiteralPath $folder -Force | Where-Object {$_.Name -notin $group.Keep})}
    }
}

function New-PublishPlan($Targets) {
    $roots=@(); $archives=@()
    foreach($target in $Targets) {
        Assert-PublishRepository $target
        if($target.Root -in $roots) { throw 'Duplicate repository root.' }
        foreach($root in $roots) {
            if($target.Root.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or
                $root.StartsWith($target.Root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Nested repositories are not supported.' }
        }
        $roots+= $target.Root
        $target.Plan=@(Get-PublishGroups $target)
        $target.DeferredPaths=@(Get-PublishDeferredPaths $target)
        foreach($group in $target.Plan) {
            if($group.Archive -in $archives) { throw 'Duplicate archive label.' }
            $archives+=$group.Archive
        }
    }
    return ,$Targets
}

function Move-PublishOutputs($Target,[string]$Backup) {
    foreach($group in $Target.Plan) {
        Assert-PublishTree $Target.Root $group.Folder -Recursive
        $destination=Join-Path $Backup $group.Archive
        Assert-PublishTree $Backup $destination
        [void](New-Item -ItemType Directory -Path $destination -Force)
        foreach($item in $group.Move) {
            $source=Join-Path $group.Folder $item.Name
            Assert-PublishTree $group.Folder $source -Recursive
            Assert-PublishTree $Backup $destination
            Move-Item -LiteralPath $source -Destination $destination -ErrorAction Stop
            Write-Host "Archived: $source"
        }
        $remaining=@(Get-ChildItem -LiteralPath $group.Folder -Force)
        if($remaining.Count -ne $group.Keep.Count) { throw "Build output changed during pruning: $($group.Folder)" }
        foreach($keep in $group.Keep) {
            $path=Join-Path $group.Folder $keep.Name
            if((Get-FileHash -LiteralPath $path).Hash -cne $keep.Hash) { throw "Delivery changed during publication: $path" }
        }
    }
}

function Assert-PublishIndex($Target) {
    foreach($path in (Invoke-PublishGit $Target.Root @('diff','--cached','--name-only','-z')).Text.Split([char]0)) {
        if($path -and $Target.DeferredPattern -and $path -match $Target.DeferredPattern) {
            throw "Deferred development file entered the index: $path"
        }
    }
    foreach($group in $Target.Plan) {
        $relative=$group.Relative.Replace('\','/')
        $tracked=@((Invoke-PublishGit $Target.Root @('ls-files','-z','--',$relative)).Text.Split([char]0) | Where-Object {$_})
        $wanted=@($group.Keep | ForEach-Object {$relative+'/'+$_.Name})
        if($tracked.Count -ne $wanted.Count -or @($tracked | Where-Object {$_ -notin $wanted}).Count) {
            throw "Index contains unexpected/missing DefaultBuild deliveries: $($Target.Name)/$relative"
        }
    }
    Assert-PublishLocalOnlyData $Target
    $added=(Invoke-PublishGit $Target.Root @('diff','--cached','--name-only','--diff-filter=ACMR','-z')).Text.Split([char]0)
    foreach($path in $added) {
        if($path -and (Get-Item -LiteralPath (Join-Path $Target.Root $path)).Length -ge 100MB) {
            throw "GitHub file size limit: $path"
        }
    }
}

function Invoke-GithubPublication($Targets,[string]$Backup,[string]$Message,
    [string]$ArchiveOwnerRoot='', [scriptblock]$ReleaseGuard=$null) {
    if(!$ArchiveOwnerRoot) {
        $tool=@($Targets | Where-Object Name -eq 'simple_renesas_isp_tool')
        if($tool.Count -ne 1) { throw 'An explicit archive-owner root is required for a scoped publication.' }
        $ArchiveOwnerRoot=$tool[0].Root
    }
    $archiveRoot=Join-Path $ArchiveOwnerRoot 'tmp\internal\github_publish'
    Assert-PublishTree $ArchiveOwnerRoot $Backup
    Assert-PublishTree $archiveRoot $Backup
    if($Backup -ieq $archiveRoot) { throw 'Use a dedicated publication backup directory.' }
    Assert-PublishAncestry $Targets
    # Recheck all roots before the first move; no cleanup if remote preflight fails.
    foreach($target in $Targets) {
        $head=$target.Head; $status=$target.Status
        Assert-PublishRepository $target
        if($target.Head -cne $head -or $target.Status -cne $status) { throw "Git state changed after approval: $($target.Name)" }
        $fresh=@(Get-PublishGroups $target)
        for($g=0;$g -lt $fresh.Count;$g++) {
            for($k=0;$k -lt $fresh[$g].Keep.Count;$k++) {
                if($fresh[$g].Keep[$k].Hash -cne $target.Plan[$g].Keep[$k].Hash) {
                    throw "Delivery changed after approval: $($target.Name)"
                }
            }
        }
        $target.Plan=$fresh
    }
    if($ReleaseGuard) { & $ReleaseGuard }
    foreach($target in $Targets) { Move-PublishOutputs $target $Backup }
    foreach($target in $Targets) {
        $stageArguments=@('add','--all','--','.')
        foreach($deferred in @(Get-PublishDeferredPaths $target)) {
            $stageArguments+= ':(literal,exclude)'+$deferred
        }
        [void](Invoke-PublishGit $target.Root $stageArguments)
        foreach($group in $target.Plan) {
            foreach($keep in $group.Keep) {
                [void](Invoke-PublishGit $target.Root @('add','-f','--',($group.Relative+'/'+$keep.Name)))
            }
        }
        Assert-PublishIndex $target
    }
    if($ReleaseGuard) { & $ReleaseGuard }
    foreach($target in $Targets) {
        if((Invoke-PublishGit $target.Root @('diff','--quiet','--no-ext-diff') @(0,1)).Code) {
            throw "Working files changed while staging the reviewed release: $($target.Name)"
        }
    }
    foreach($target in $Targets) {
        if((Invoke-PublishGit $target.Root @('diff','--cached','--quiet') @(0,1)).Code) {
            [void](Invoke-PublishGit $target.Root @('commit','-m',$Message))
        }
        $target.Head=(Invoke-PublishGit $target.Root @('rev-parse','HEAD')).Text
        Write-Host "COMMITTED $($target.Name) $($target.Head)"
    }
    foreach($target in $Targets) {
        Write-Host "PUSHING $($target.Name) $($target.Head)"
        [void](Invoke-PublishGit $target.Root @('push','--porcelain','--no-force','--no-follow-tags','--recurse-submodules=no','origin',($target.Head+':refs/heads/main')))
        $remote=(Invoke-PublishGit $target.Root @('ls-remote','--exit-code','origin','refs/heads/main')).Text.Split("`t")[0]
        if($remote -cne $target.Head) { throw "Remote verification failed: $($target.Name)" }
        Write-Host "PUSH VERIFIED $($target.Name) $($target.Head)"
    }
}
