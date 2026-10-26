[CmdletBinding()]
param([switch]$Apply)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:PrerequisiteProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$script:PrerequisiteManifest = Join-Path $PSScriptRoot 'prerequisite_cleanup_manifest.json'

function Assert-PrerequisiteProjectPath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($script:PrerequisiteProject + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the selected project: $full"
    }
    $ancestor = $full
    while ($ancestor) {
        if (Test-Path -LiteralPath $ancestor) {
            $item = Get-Item -LiteralPath $ancestor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse path refused: $ancestor"
            }
        }
        $ancestor = [IO.Path]::GetDirectoryName($ancestor)
    }
    return $full
}

function ConvertTo-PrerequisiteRelativePath([string]$Path) {
    $relative = $Path.Replace('/', '\')
    if ($relative -notmatch '^tmp\\internal\\tests\\can_prerequisite_(Debug|Release)_[0-9a-f]{8}\\(can_prerequisite_ui_tests\.obj|prerequisite_fixture\.(exe|ilk|pdb))$') {
        throw "Unapproved prerequisite artifact: $Path"
    }
    return $relative
}

function ConvertTo-PrerequisiteEntries([object[]]$Records) {
    $entries = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($record in $Records) {
        $relative = ConvertTo-PrerequisiteRelativePath ([string]$record.path)
        if ([long]$record.bytes -lt 0 -or [string]$record.sha256 -notmatch '^[0-9a-f]{64}$') {
            throw "Invalid artifact metadata: $relative"
        }
        if ($entries.ContainsKey($relative)) { throw "Duplicate artifact: $relative" }
        $entries.Add($relative, $record)
    }
    return ,$entries
}

function Get-ReviewedPrerequisiteArtifact([string]$Path, [object]$Record) {
    $full = Assert-PrerequisiteProjectPath $Path
    $item = Get-Item -LiteralPath $full -Force
    if ($item.PSIsContainer -or $item.Length -ne [long]$Record.bytes) {
        throw "Changed artifact retained: $full"
    }
    $hash = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    if ($hash -ne [string]$Record.sha256) { throw "Changed artifact hash; retained: $full" }
    return [pscustomobject]@{ Path=$full; Bytes=$item.Length; Hash=$hash; WriteTicks=$item.LastWriteTimeUtc.Ticks }
}

function Find-PrerequisiteProcesses([object[]]$Processes, [string[]]$RunDirectories) {
    foreach ($process in $Processes) {
        if (-not $process.ExecutablePath) { continue }
        foreach ($directory in $RunDirectories) {
            if ($process.ExecutablePath.StartsWith($directory + '\', [StringComparison]::OrdinalIgnoreCase)) {
                $process
                break
            }
        }
    }
}

function Get-PrerequisiteCleanupPlan {
    $projectFile = Assert-PrerequisiteProjectPath (Join-Path $script:PrerequisiteProject 'FirmwareUpgradeTool.vcxproj')
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) { throw 'Project identity file is missing.' }
    $manifestPath = Assert-PrerequisiteProjectPath $script:PrerequisiteManifest
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.schema -ne 1 -or $manifest.generator.path -ne 'scripts/test_can_prerequisite_ui.ps1') {
        throw 'Unsupported prerequisite cleanup manifest.'
    }
    $generator = Assert-PrerequisiteProjectPath (Join-Path $script:PrerequisiteProject 'scripts\test_can_prerequisite_ui.ps1')
    if ((Get-FileHash -LiteralPath $generator -Algorithm SHA256).Hash -ne $manifest.generator.sha256) {
        throw 'Fixture generator changed. Review the cleanup manifest before applying it.'
    }
    $entries = ConvertTo-PrerequisiteEntries @($manifest.files)
    $approvedRuns = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $entries.Keys) { [void]$approvedRuns.Add([IO.Path]::GetDirectoryName($relative)) }
    $testRoot = Assert-PrerequisiteProjectPath (Join-Path $script:PrerequisiteProject 'tmp\internal\tests')
    $files = [Collections.Generic.List[object]]::new()
    $runs = [Collections.Generic.List[string]]::new()
    $retained = 0
    $unreviewedRuns = 0
    if (Test-Path -LiteralPath $testRoot) {
        foreach ($directory in Get-ChildItem -LiteralPath $testRoot -Directory -Force | Sort-Object Name) {
            if ($directory.Name -notmatch '^can_prerequisite_(Debug|Release)_[0-9a-f]{8}$') { continue }
            $run = Assert-PrerequisiteProjectPath $directory.FullName
            $relativeRun = $run.Substring($script:PrerequisiteProject.Length + 1)
            if (-not $approvedRuns.Contains($relativeRun)) { $unreviewedRuns++; continue }
            $runs.Add($run)
            foreach ($child in Get-ChildItem -LiteralPath $run -Force) {
                [void](Assert-PrerequisiteProjectPath $child.FullName)
                $relative = $child.FullName.Substring($script:PrerequisiteProject.Length + 1)
                if ($entries.ContainsKey($relative)) {
                    $files.Add((Get-ReviewedPrerequisiteArtifact $child.FullName $entries[$relative]))
                } else { $retained++ }
            }
        }
    }
    $bytes = [long]0
    foreach ($file in $files) { $bytes += $file.Bytes }
    return [pscustomobject]@{
        Files=@($files.ToArray()); RunDirectories=@($runs.ToArray()); Bytes=$bytes
        RetainedEntries=$retained; UnreviewedRuns=$unreviewedRuns
        ManifestHash=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    }
}

function Invoke-PrerequisiteCleanup([switch]$Apply) {
    $plan = Get-PrerequisiteCleanupPlan
    $mode = if ($Apply) { 'APPLY' } else { 'PREVIEW' }
    Write-Host ("{0}: {1} reviewed files; {2} bytes ({3:N2} MB)." -f $mode,$plan.Files.Count,$plan.Bytes,($plan.Bytes / 1000000))
    Write-Host ("Retained: {0} other entries; {1} unreviewed run directories." -f $plan.RetainedEntries,$plan.UnreviewedRuns)
    if (-not $Apply) {
        foreach ($file in $plan.Files) { Write-Host ('WOULD REMOVE ' + $file.Path) }
        Write-Host 'Preview only. No files or directories changed.'
        return
    }
    $processes = @(Find-PrerequisiteProcesses @(Get-CimInstance Win32_Process) $plan.RunDirectories)
    if ($processes.Count) { throw 'A fixture process is active. Close it before cleaning; no process was stopped.' }
    $manifestPath = Assert-PrerequisiteProjectPath $script:PrerequisiteManifest
    if ((Get-FileHash -LiteralPath $manifestPath).Hash -ne $plan.ManifestHash) { throw 'Cleanup manifest changed.' }
    # Revalidate the complete plan before the first deletion.
    foreach ($file in $plan.Files) {
        $current = Get-ReviewedPrerequisiteArtifact $file.Path ([pscustomobject]@{ bytes=$file.Bytes; sha256=$file.Hash })
        if ($current.WriteTicks -ne $file.WriteTicks) { throw "Artifact changed during preflight: $($file.Path)" }
    }
    $removed = 0
    $removedBytes = [long]0
    $emptyDirectories = 0
    try {
        foreach ($file in $plan.Files) {
            $path = Assert-PrerequisiteProjectPath $file.Path
            $current = Get-ReviewedPrerequisiteArtifact $path ([pscustomobject]@{ bytes=$file.Bytes; sha256=$file.Hash })
            if ($current.WriteTicks -ne $file.WriteTicks) { throw "Artifact changed before removal: $path" }
            Remove-Item -LiteralPath $path -Force -ErrorAction Stop
            $removed++
            $removedBytes += $file.Bytes
        }
        foreach ($directory in $plan.RunDirectories) {
            $path = Assert-PrerequisiteProjectPath $directory
            if (@(Get-ChildItem -LiteralPath $path -Force).Count -eq 0) {
                # Atomic empty-only removal; a newly added file makes this fail.
                [IO.Directory]::Delete($path, $false)
                $emptyDirectories++
            }
        }
    } catch {
        Write-Host ("Stopped after {0} files / {1} bytes removed." -f $removed,$removedBytes)
        throw
    }
    Write-Host ("Removed {0} files / {1} bytes; {2} empty run directories." -f $removed,$removedBytes,$emptyDirectories)
    Write-Host 'INI, screenshots, logs, unlisted files and nonempty directories were retained.'
}

# Dot-sourcing loads only the read-only planning/validation functions for tests.
if ($MyInvocation.InvocationName -eq '.') { return }
try {
    Invoke-PrerequisiteCleanup -Apply:$Apply
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
