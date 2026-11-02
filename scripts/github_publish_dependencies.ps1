# Read-only closure of the actual PC product's compile inputs and quoted includes.
# Withheld firmware is never selected implicitly to satisfy a build dependency.
function Get-PublishToolDependencies($Target) {
    $project=Join-Path $Target.Root 'FirmwareUpgradeTool.vcxproj'
    Assert-PublishTree $Target.Root $project
    if(!(Test-Path -LiteralPath $project -PathType Leaf)) { throw 'Tool build dependency check requires FirmwareUpgradeTool.vcxproj.' }
    [xml]$xml=Get-Content -LiteralPath $project -Raw -Encoding UTF8
    $queue=New-Object 'Collections.Generic.Queue[string]'
    foreach($node in $xml.SelectNodes('//*[local-name()="ClCompile" or local-name()="ResourceCompile"][@Include]')) {
        $relative=[string]$node.Include
        if($relative -match '\$\(' -or [IO.Path]::IsPathRooted($relative)) { throw "Unsupported nonlocal compile dependency: $relative" }
        $queue.Enqueue([IO.Path]::GetFullPath((Join-Path $Target.Root $relative)))
    }
    $seen=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $dependencies=@()
    while($queue.Count) {
        $full=$queue.Dequeue()
        if(!$seen.Add($full)) { continue }
        Assert-PublishTree $Target.Root $full
        if(!(Test-Path -LiteralPath $full -PathType Leaf)) { throw "Missing Tool build dependency: $full" }
        $relative=$full.Substring($Target.Root.TrimEnd('\','/').Length+1).Replace('\','/')
        $selected=Test-ToolPublicationPath $relative
        $baseline=''
        if(!$selected) {
            # Normalized blob equality handles Git's CRLF checkout conversion.
            # Outgoing-history review separately proves this HEAD base is not a
            # new withheld commit being pushed by this publication.
            $baseline=(Invoke-PublishGit $Target.Root @('rev-parse','--verify',('HEAD:'+$relative)) @(0,128)).Text
            if(!$baseline) { throw "Withheld Tool build dependency is absent from the accepted base: $relative. Review scope; it will not be uploaded automatically." }
            $local=(Invoke-PublishGit $Target.Root @('hash-object',('--path='+$relative),'--',$relative)).Text
            if($local -cne $baseline) { throw "Withheld Tool build dependency changed: $relative. Resolve the release scope before publishing dependent PC source." }
        }
        $dependencies+=[ordered]@{Path=$relative;Sha256=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash;Selected=$selected;BaselineBlob=$baseline}
        $source=Get-Content -LiteralPath $full -Raw -Encoding UTF8
        foreach($include in [regex]::Matches($source,'(?m)^\s*#\s*include\s*"([^"\r\n]+)"')) {
            $name=$include.Groups[1].Value
            if([IO.Path]::IsPathRooted($name)) { throw "Absolute Tool include is not portable: $relative -> $name" }
            $candidates=@((Split-Path -Parent $full),$Target.Root,(Join-Path $Target.Root 'src'),
                (Join-Path $Target.Root 'src\core'),(Join-Path $Target.Root 'firmware\common'))
            $resolved=$null
            foreach($directory in $candidates) {
                $candidate=[IO.Path]::GetFullPath((Join-Path $directory $name))
                Assert-PublishTree $Target.Root $candidate
                if(Test-Path -LiteralPath $candidate -PathType Leaf) { $resolved=$candidate; break }
            }
            if(!$resolved) { throw "Missing quoted Tool include: $relative -> $name" }
            $queue.Enqueue($resolved)
        }
    }
    return @($dependencies | Sort-Object { $_.Path })
}
