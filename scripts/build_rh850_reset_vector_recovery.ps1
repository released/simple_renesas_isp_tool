param(
    [Parameter(Mandatory = $true)][string]$SdkProject,
    [string]$CompilerBin = '',
    [switch]$TestOnly
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repo 'firmware\diagnostics\rh850_reset_vector'
$sdk = (Resolve-Path -LiteralPath $SdkProject).Path
if (!$CompilerBin) {
    $CompilerBin = Join-Path ${env:ProgramFiles(x86)} 'Renesas Electronics\CS+\CC\CC-RH\V2.07.00\bin'
}
$compiler = Join-Path $CompilerBin 'ccrh.exe'
$assembler = Join-Path $CompilerBin 'asrh.exe'
$linker = Join-Path $CompilerBin 'rlink.exe'
foreach ($path in @($compiler, $assembler, $linker, (Join-Path $sdk 'iodefine.h'))) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required existing input not found: $path"
    }
}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope = New-OwnedTestWorkspace 'rh850_reset_vector_build' -ShortPath
$run = (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$out = Join-Path $repo ((@{ $true = 'tmp\internal\verification'; $false = 'build\rh850_reset_vector' }[$TestOnly.IsPresent]) + '\' + $run)
if (Test-Path -LiteralPath $out) { throw 'Output must be fresh.' }
[IO.Directory]::CreateDirectory($out) | Out-Null
$sdkInputs = @((Join-Path $sdk 'iodefine.h')) + @(
    Get-ChildItem -LiteralPath (Join-Path $sdk 'FCL') -Recurse -File |
        Where-Object { $_.Extension -in '.c', '.h', '.asm' } |
        Select-Object -ExpandProperty FullName
)
$before = @{}
foreach ($path in $sdkInputs) { $before[$path] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
$log = New-Object Collections.Generic.List[string]
$result = [ordered]@{ Purpose = 'R7F701684 RAM-only Reset Vector recovery'; Hardware = 'NOT RUN'; Result = 'FAIL'; Output = $out }
function Invoke-RvTool([string]$Exe, [string[]]$Arguments) {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $messages = & $Exe @Arguments 2>&1; $code = $LASTEXITCODE }
    finally { $ErrorActionPreference = $savedPreference }
    foreach ($message in $messages) { $log.Add($message.ToString()) }
    if ($code) { throw ((Split-Path $Exe -Leaf) + ' failed: ' + ($messages -join "`n")) }
}
Push-Location -LiteralPath $scope.Path
try {
    $includes = @($source, $sdk, (Join-Path $sdk 'FCL'), (Join-Path $sdk 'FCL\lib')) | ForEach-Object { '-I' + $_ }
    $units = [ordered]@{
        recovery = (Join-Path $source 'recovery.c')
        platform = (Join-Path $source 'platform.c')
        r_fcl_user_if = (Join-Path $sdk 'FCL\lib\r_fcl_user_if.c')
        r_fcl_hw_access = (Join-Path $sdk 'FCL\lib\r_fcl_hw_access.c')
    }
    $objects = @()
    foreach ($unit in $units.GetEnumerator()) {
        $object = $unit.Key + '.obj'
        Invoke-RvTool $compiler (@($unit.Value, ('-o' + $object), '-Xcpu=g3kh', '-g', '-g_line', '-Xalign4', '-Xdbl_size=4', '-lang=c', '-Xasm_path=.', '-c') + $includes)
        $objects += $object
    }
    foreach ($unit in @(@('entry', (Join-Path $source 'entry.asm')), @('r_fcl_hw_access_asm', (Join-Path $sdk 'FCL\lib\r_fcl_hw_access_asm.asm')))) {
        Invoke-RvTool $assembler @($unit[1], ('-o' + $unit[0] + '.obj'), '-Xcpu=g3kh', '-g')
        $objects += $unit[0] + '.obj'
    }
    $start = 'RV_ENTRY/FEDE0000,RV_VECTORS/FEDE0200,.text,.const,R_FCL_CODE_ROM.text,R_FCL_CONST.const,R_FCL_CODE_USRINT.text,R_FCL_CODE_USR.text,R_FCL_CODE_RAM.text,R_FCL_CODE_ROMRAM.text,R_FCL_CODE_RAM_EX_PROT.text/FEDE1000,.bss,R_FCL_DATA.bss/FEDF0000,RV_STACK.bss/FEDF2000,RV_MAILBOX.bss/FEDF7000,RV_FCL_COPY.bss/FEDF8000'
    $linkArgs = @($objects)
    $linkArgs += @(('-start=' + $start), '-debug', '-nocompress', '-NOOPtimize', '-output=recovery.abs', '-list=recovery.map', '-show=all', '-total_size', '-nologo')
    Invoke-RvTool $linker $linkArgs
    Invoke-RvTool $linker @('recovery.abs', '-form=stype', '-output=recovery.mot', '-nologo')
    $map = [IO.File]::ReadAllText((Join-Path $scope.Path 'recovery.map'))
    $mapping = $map.Substring($map.IndexOf('*** Mapping List ***'))
    $mapping = $mapping.Substring(0, $mapping.IndexOf('*** Total Section Size ***'))
    $sections = @()
    foreach ($match in [regex]::Matches($mapping, '(?m)^([A-Za-z_.][A-Za-z0-9_.]*)\s*\r?\n\s*([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]+)\s+')) {
        $sections += [pscustomobject]@{ Name = $match.Groups[1].Value; Start = [Convert]::ToUInt32($match.Groups[2].Value, 16); End = [Convert]::ToUInt32($match.Groups[3].Value, 16); Size = [Convert]::ToUInt32($match.Groups[4].Value, 16) }
    }
    if ($sections.Count -lt 12) { throw 'Incomplete actual linker mapping.' }
    foreach ($section in $sections) {
        if (!$section.Size) { continue }
        $valid = ($section.Start -ge 0xFEDE0000L -and $section.End -lt 0xFEDE8000L) -or
            ($section.Start -ge 0xFEDF0000L -and $section.End -lt 0xFEDF2000L) -or
            ($section.Name -eq 'RV_STACK.bss' -and $section.Start -eq 0xFEDF2000L -and $section.End -eq 0xFEDF2FFFL) -or
            ($section.Name -eq 'RV_MAILBOX.bss' -and $section.Start -eq 0xFEDF7000L -and $section.End -lt 0xFEDF7100L) -or
            ($section.Name -eq 'RV_FCL_COPY.bss' -and $section.Start -eq 0xFEDF8000L -and $section.End -eq 0xFEDFEFFFL)
        if (!$valid) { throw ('Unexpected linked section / debugger RAM overlap: ' + $section.Name) }
    }
    foreach ($symbol in @{ '_RvEntry' = 'fede0000'; '_g_rv' = 'fedf7000'; '_RvStackTop' = 'fedf3000' }.GetEnumerator()) {
        if ($map -notmatch ('(?m)^\s*' + [regex]::Escape($symbol.Key) + '\s*\r?\n\s*' + $symbol.Value + '\b')) { throw ('Unexpected symbol: ' + $symbol.Key) }
    }
    $copyBytes = ($sections | Where-Object { $_.Name -in 'R_FCL_CODE_USRINT.text', 'R_FCL_CODE_USR.text', 'R_FCL_CODE_RAM.text', 'R_FCL_CODE_ROMRAM.text', 'R_FCL_CODE_RAM_EX_PROT.text' } | Measure-Object Size -Sum).Sum
    if ($copyBytes -ge 0x7000) { throw 'FCL copy exceeds reserved RAM.' }
    $payload = 0
    foreach ($line in [IO.File]::ReadLines((Join-Path $scope.Path 'recovery.mot'))) {
        if ($line -notmatch '^S[123]') { continue }
        $addressBytes = [int]$line.Substring(1, 1) + 1
        $count = [Convert]::ToInt32($line.Substring(2, 2), 16)
        if ($line.Length -ne 4 + 2 * $count) { throw 'Bad generated S-record length.' }
        $sum = $count
        for ($i = 4; $i -lt $line.Length; $i += 2) { $sum += [Convert]::ToInt32($line.Substring($i, 2), 16) }
        if (($sum -band 255) -ne 255) { throw 'Bad generated S-record checksum.' }
        $address = [Convert]::ToUInt32($line.Substring(4, 2 * $addressBytes), 16)
        $length = $count - $addressBytes - 1
        if ($address -lt 0xFEDE0000L -or $address + $length -gt 0xFEDE8000L) { throw 'Load payload outside approved RAM; never download this image.' }
        $payload += $length
    }
    if ($payload -lt 1000) { throw 'Unexpected empty RAM image.' }
    $result.Result = 'PASS'
    $result.LoadedRamBytes = $payload
    $result.FclCopyBytes = $copyBytes
    $result.Sections = $sections
    $result.SdkInputHashes = $before
    $result.SourceHashes = @(Get-ChildItem -LiteralPath $source -File | Get-FileHash -Algorithm SHA256 | Select-Object Path, Hash)
    $result.Warnings = @($log | Where-Object { $_ -match 'W\d{7}|warning' })
    if (!$TestOnly) {
        foreach ($file in @('recovery.abs', 'recovery.map', 'recovery.mot')) {
            [IO.File]::Copy((Join-Path $scope.Path $file), (Join-Path $out $file), $false)
        }
        $result.DeliveryHashes = @(Get-ChildItem -LiteralPath $out -File | Get-FileHash -Algorithm SHA256 | Select-Object Path, Hash)
    }
    Write-Host ('PASS: RAM-only image; ' + $payload + ' load bytes; hardware NOT RUN. ' + $out)
} catch {
    $result.Error = $_.Exception.Message
    throw
} finally {
    Pop-Location
    foreach ($path in $sdkInputs) {
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $before[$path]) { $result.Result = 'FAIL'; $result.InputChanged = $path }
    }
    [IO.File]::WriteAllText((Join-Path $out 'build.log'), ($log -join "`r`n"))
    [IO.File]::WriteAllText((Join-Path $out 'evidence.json'), ($result | ConvertTo-Json -Depth 8))
    Remove-OwnedTestWorkspace $scope
    if ($result.InputChanged) { throw 'SDK source changed during isolated build.' }
}
