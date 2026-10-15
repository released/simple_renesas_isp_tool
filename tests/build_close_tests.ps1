$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\close_project_tool.ps1')
$script:processes = @()

# Shadow enumeration only inside this test process; never start or close an app.
function Get-Process {
    param([string[]]$Name, [string]$ErrorAction)
    return $script:processes
}
function Assert-True($Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function New-TestProcess([string]$ImagePath) {
    $item = [pscustomobject]@{
        Id = 123; Path = $ImagePath; HasExited = $false; CloseCount = 0
        WaitCount = 0; CloseAccepted = $true; ExitOnWait = $true
    }
    $item | Add-Member ScriptMethod CloseMainWindow {
        $this.CloseCount++
        return $this.CloseAccepted
    }
    $item | Add-Member ScriptMethod WaitForExit {
        param([int]$Timeout)
        $this.WaitCount++
        if ($this.ExitOnWait) { $this.HasExited = $true }
        return $this.ExitOnWait
    }
    return $item
}

$testBuild = 'C:\tests\current\build'
Close-ProjectToolForBuild -BuildDirectory $testBuild
$fixed = New-TestProcess "$testBuild\FirmwareUpgradeTool.exe"
$debug = New-TestProcess "$testBuild\debug\x64\FirmwareUpgradeTool.exe"
$versioned = New-TestProcess "$testBuild\FirmwareUpgradeTool_0.1.0.0072.exe"
$foreign = New-TestProcess 'C:\tests\other\build\FirmwareUpgradeTool.exe'
$prefixCollision = New-TestProcess 'C:\tests\current\build-backup\FirmwareUpgradeTool.exe'
$unknownName = New-TestProcess "$testBuild\FirmwareUpgradeTool_notes.exe"
$missingPath = New-TestProcess ''
$script:processes = @($fixed, $debug, $versioned, $foreign, $prefixCollision, $unknownName, $missingPath)
Close-ProjectToolForBuild -BuildDirectory $testBuild.ToUpperInvariant()
foreach ($item in @($fixed, $debug, $versioned)) {
    Assert-True ($item.CloseCount -eq 1 -and $item.WaitCount -eq 1 -and $item.HasExited) 'Expected one graceful close and wait.'
}
foreach ($item in @($foreign, $prefixCollision, $unknownName, $missingPath)) {
    Assert-True ($item.CloseCount -eq 0 -and $item.WaitCount -eq 0) 'An unrelated process was touched.'
}
Close-ProjectToolForBuild -BuildDirectory $testBuild
Assert-True ($fixed.CloseCount -eq 1) 'Already-exited process was closed twice.'

$unreadable = New-TestProcess "$testBuild\FirmwareUpgradeTool.exe"
$unreadable.PSObject.Properties.Remove('Path')
$unreadable | Add-Member ScriptProperty Path { throw 'Access denied' }
$script:processes = @($unreadable)
Close-ProjectToolForBuild -BuildDirectory $testBuild -WarningAction SilentlyContinue
Assert-True ($unreadable.CloseCount -eq 0) 'Unknown executable identity must not be closed.'

$noWindow = New-TestProcess "$testBuild\FirmwareUpgradeTool.exe"
$noWindow.CloseAccepted = $false
$script:processes = @($noWindow)
$caught = $false
try { Close-ProjectToolForBuild -BuildDirectory $testBuild }
catch { $caught = $_.Exception.Message -match 'Build stopped.*No process was force-terminated' }
Assert-True ($caught -and $noWindow.WaitCount -eq 0) 'Rejected close must fail the build safely.'

$slow = New-TestProcess "$testBuild\FirmwareUpgradeTool.exe"
$slow.ExitOnWait = $false
$script:processes = @($slow)
$caught = $false
try { Close-ProjectToolForBuild -BuildDirectory $testBuild -TimeoutMilliseconds 1 }
catch { $caught = $_.Exception.Message -match 'Build stopped.*within 1 ms' }
Assert-True ($caught -and -not $slow.HasExited -and $slow.WaitCount -eq 1) 'Timeout must not force termination.'

$racing = New-TestProcess "$testBuild\FirmwareUpgradeTool.exe"
$racing | Add-Member ScriptMethod CloseMainWindow {
    $this.CloseCount++
    $this.HasExited = $true
    return $false
} -Force
$script:processes = @($racing)
Close-ProjectToolForBuild -BuildDirectory $testBuild
Assert-True ($racing.HasExited -and $racing.CloseCount -eq 1) 'Exit during close should be accepted.'

Write-Host 'PASS: build-close mocks (fixed/Debug/versioned, foreign/prefix/name/unknown, idempotent/race, reject/timeout). No real process started or closed.'
