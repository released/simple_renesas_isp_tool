# Isolated entry-gate checks only; no real repositories, network or hardware.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$entry=Join-Path $PSScriptRoot 'push_github.ps1'
$parseTokens=$null; $parseErrors=$null
[void][Management.Automation.Language.Parser]::ParseFile($entry,[ref]$parseTokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Publication entry has PowerShell parse errors.'}
$testRoot=Join-Path $repo ('tmp\internal\verification\firmware-publication-hold\'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $testRoot -Force)
$results=New-Object 'Collections.Generic.List[object]'
foreach($nonInteractive in @($false,$true)) {
    $caseName=if($nonInteractive){'execute-yes'}else{'execute'}
    $caseRoot=Join-Path $testRoot ($caseName+' with spaces')
    [void](New-Item -ItemType Directory -Path (Join-Path $caseRoot 'scripts') -Force)
    $copy=Join-Path $caseRoot 'scripts\push_github.ps1'
    Copy-Item -LiteralPath $entry -Destination $copy
    if((Get-FileHash -LiteralPath $entry).Hash -cne (Get-FileHash -LiteralPath $copy).Hash){throw 'Fixture is not byte-matched.'}
    $protected=@('.git\index','build\FirmwareUpgradeTool.exe','build\tool.ini','firmware\untested\source.c')
    foreach($relative in $protected) {
        $path=Join-Path $caseRoot $relative
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force)
        [IO.File]::WriteAllText($path,('synthetic protected fixture: '+$relative))
    }
    # If the gate is too late, fail safely before any real path/Git operations.
    [IO.File]::WriteAllText((Join-Path $caseRoot 'scripts\workspace_paths.ps1'),"throw 'DEPENDENCY_LOADED_BEFORE_HARDWARE_HOLD'")
    $before=@{}
    foreach($file in (Get-ChildItem -LiteralPath $caseRoot -Recurse -File)){$before[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\powershell.exe'
    $info.Arguments='-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$copy+'" -Execute'
    if($nonInteractive){$info.Arguments+=' -Yes'}
    $info.WorkingDirectory=$caseRoot
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    $process=New-Object Diagnostics.Process
    $process.StartInfo=$info
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync()
        $stderr=$process.StandardError.ReadToEndAsync()
        if(!$process.WaitForExit(15000)){$process.Kill();throw 'Owned fixture process timed out.'}
        $output=$stdout.Result+"`n"+$stderr.Result
        [IO.File]::WriteAllText((Join-Path $testRoot ($caseName+'.log')),$output)
        if($process.ExitCode -eq 0 -or $output -notmatch 'MCU firmware publication is on hold') {throw "Missing hardware-publication refusal: $caseName"}
        if($output -match 'DEPENDENCY_LOADED_BEFORE_HARDWARE_HOLD'){throw 'Gate ran after dependency loading.'}
        $after=@(Get-ChildItem -LiteralPath $caseRoot -Recurse -File)
        if($after.Count -ne $before.Count){throw 'Gate created or removed fixture files.'}
        foreach($file in $after) {
            if(!$before.ContainsKey($file.FullName) -or $before[$file.FullName] -cne (Get-FileHash -LiteralPath $file.FullName).Hash){throw 'Protected fixture changed.'}
        }
        $results.Add([pscustomobject]@{Case=$caseName;Result='PASS';ExitCode=$process.ExitCode;FilesPreserved=$before.Count;DependenciesLoaded=$false})
        Write-Host "PASS: $caseName refused before dependencies; protected files/index unchanged."
    } finally {$process.Dispose()}
}
ConvertTo-Json -InputObject ($results.ToArray()) -Depth 4|Set-Content -LiteralPath (Join-Path $testRoot 'result.json') -Encoding UTF8
Write-Host "2 scoped entry-gate checks PASS. No actual GitHub/MCU/product build/clean operation. Evidence: $testRoot"
