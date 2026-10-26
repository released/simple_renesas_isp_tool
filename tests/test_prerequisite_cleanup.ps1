$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\clean_prerequisite_tests.ps1')
$checks = 0
function Assert-Check([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "FAIL: $Name" }
    $script:checks++
}
function Assert-Refused([scriptblock]$Action, [string]$Name) {
    $refused = $false
    try { & $Action | Out-Null } catch { $refused = $true }
    Assert-Check $refused $Name
}

$valid = 'tmp/internal/tests/can_prerequisite_Debug_1234abcd/prerequisite_fixture.exe'
Assert-Check ((ConvertTo-PrerequisiteRelativePath $valid) -eq $valid.Replace('/','\')) 'approved fixture path'
foreach ($bad in @(
    'C:\Windows\prerequisite_fixture.exe',
    '..\simple_capture_tool\prerequisite_fixture.exe',
    'tmp/internal/tests/../prerequisite_fixture.exe',
    'tmp/internal/tests/can_prerequisite_Debug_1234abcd/FirmwareUpgradeTool.ini',
    'tmp/internal/tests/can_prerequisite_Debug_1234abcd/nested/prerequisite_fixture.exe',
    'tmp/internal/tests/can_prerequisite_1234abcd/prerequisite_fixture.exe',
    'tmp/internal/tests/can_prerequisite_Debug_bad/prerequisite_fixture.exe'
)) { Assert-Refused { ConvertTo-PrerequisiteRelativePath $bad } ('unsafe relative path: ' + $bad) }
Assert-Refused { Assert-PrerequisiteProjectPath $script:PrerequisiteProject } 'project root cannot be a target'
Assert-Refused { Assert-PrerequisiteProjectPath (Join-Path $script:PrerequisiteProject '..\sibling\file.obj') } 'sibling boundary'
Assert-Refused { Assert-PrerequisiteProjectPath ($script:PrerequisiteProject + '-other\file.obj') } 'same-prefix sibling boundary'
& {
    # Exercise the rejection branch without creating a filesystem junction.
    function Test-Path { param([string]$LiteralPath) return $true }
    function Get-Item {
        param([string]$LiteralPath, [switch]$Force)
        return [pscustomobject]@{ Attributes=[IO.FileAttributes]::ReparsePoint }
    }
    Assert-Refused { Assert-PrerequisiteProjectPath (Join-Path $script:PrerequisiteProject 'FirmwareUpgradeTool.vcxproj') } 'mocked reparse path'
}

$record = [pscustomobject]@{ path=$valid; bytes=1; sha256=('A' * 64) }
Assert-Check ((ConvertTo-PrerequisiteEntries @($record)).Count -eq 1) 'valid reviewed record'
Assert-Refused { ConvertTo-PrerequisiteEntries @($record,$record) } 'duplicate record'
Assert-Refused { ConvertTo-PrerequisiteEntries @([pscustomobject]@{ path=$valid; bytes=-1; sha256=('A' * 64) }) } 'negative size'
Assert-Refused { ConvertTo-PrerequisiteEntries @([pscustomobject]@{ path=$valid; bytes=1; sha256='invalid' }) } 'invalid hash'

$run = Join-Path $script:PrerequisiteProject 'tmp\internal\tests\can_prerequisite_Debug_1234abcd'
$processes = @(
    [pscustomobject]@{ ExecutablePath=($run + '\prerequisite_fixture.exe') },
    [pscustomobject]@{ ExecutablePath=($run + '-other\prerequisite_fixture.exe') },
    [pscustomobject]@{ ExecutablePath=(Join-Path $script:PrerequisiteProject 'build\FirmwareUpgradeTool.exe') },
    [pscustomobject]@{ ExecutablePath=$null }
)
Assert-Check (@(Find-PrerequisiteProcesses $processes @($run)).Count -eq 1) 'process guard distinguishes unrelated same-name paths'

$plan = Get-PrerequisiteCleanupPlan
Assert-Check ($plan.UnreviewedRuns -ge 0) 'read-only plan handles unreviewed runs'
foreach ($file in $plan.Files) {
    Assert-Check ($file.Path.StartsWith($script:PrerequisiteProject + '\',[StringComparison]::OrdinalIgnoreCase)) 'planned path inside project'
}
if ($plan.Files.Count) {
    $first = $plan.Files[0]
    Assert-Refused { Get-ReviewedPrerequisiteArtifact $first.Path ([pscustomobject]@{ bytes=($first.Bytes+1); sha256=$first.Hash }) } 'changed file size retained'
    Assert-Refused { Get-ReviewedPrerequisiteArtifact $first.Path ([pscustomobject]@{ bytes=$first.Bytes; sha256=('0'*64) }) } 'changed file hash retained'
}
Write-Output ("PASS: {0} read-only checks; {1} current reviewed files; {2} bytes. Apply/deletion NOT RUN." -f $checks,$plan.Files.Count,$plan.Bytes)
