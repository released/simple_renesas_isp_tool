function Close-ProjectToolForBuild {
    param(
        [Parameter(Mandatory = $true)][string]$BuildDirectory,
        [ValidateRange(1, 60000)][int]$TimeoutMilliseconds = 15000
    )

    $buildPrefix = [System.IO.Path]::GetFullPath($BuildDirectory).TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    $running = @(Get-Process -Name 'FirmwareUpgradeTool*' -ErrorAction SilentlyContinue)
    foreach ($toolProcess in $running) {
        try {
            $imagePath = $toolProcess.Path
        } catch {
            Write-Warning "Cannot inspect process $($toolProcess.Id); leaving it running."
            continue
        }
        if ([string]::IsNullOrWhiteSpace($imagePath)) { continue }
        $imagePath = [System.IO.Path]::GetFullPath($imagePath)
        if (-not $imagePath.StartsWith($buildPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $imageName = [System.IO.Path]::GetFileName($imagePath)
        if ($imageName -notmatch '^FirmwareUpgradeTool(?:_\d\.\d\.\d\.\d{3,4})?\.exe$') {
            continue
        }
        if ($toolProcess.HasExited) { continue }
        Write-Host "Closing this project's tool before build: $imagePath (PID $($toolProcess.Id))"
        try {
            if (-not $toolProcess.CloseMainWindow() -and -not $toolProcess.HasExited) {
                throw 'The main window did not accept the close request.'
            }
            if (-not $toolProcess.WaitForExit($TimeoutMilliseconds)) {
                throw "Normal shutdown did not finish within $TimeoutMilliseconds ms."
            }
        } catch {
            if ($toolProcess.HasExited) { continue }
            throw "Build stopped: $imagePath could not close safely. No process was force-terminated. $($_.Exception.Message)"
        }
    }
}
