param([Parameter(Mandatory = $true)][string]$SdkProject, [switch]$InjectFailure)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repo 'firmware\diagnostics\rh850_reset_vector'
$sdk = (Resolve-Path -LiteralPath $SdkProject).Path
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope = New-OwnedTestWorkspace 'rh850_reset_vector_model' -ShortPath
$result = [ordered]@{ Result = 'FAIL'; Hardware = 'NOT RUN'; Scope = 'actual recovery.c; modeled FCL backend' }
$evidence = Join-Path $repo ('tmp\internal\verification\rh850_reset_vector_' + [guid]::NewGuid().ToString('N').Substring(0, 8))
[IO.Directory]::CreateDirectory($evidence) | Out-Null
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Existing MSVC is required for native verification.' }
    $lines = & cmd /d /c ('call "' + (Join-Path $vs 'Common7\Tools\VsDevCmd.bat') + '" -no_logo -arch=x86 >nul && set')
    if ($LASTEXITCODE) { throw 'MSVC x86 environment failed.' }
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    # Model only native ABI scalar typedefs; use actual vendor enums and request types.
    [IO.File]::WriteAllText((Join-Path $scope.Path 'r_typedefs.h'), "typedef unsigned char uint8_t;`ntypedef unsigned short uint16_t;`ntypedef unsigned long uint32_t;`n")
    Push-Location -LiteralPath $scope.Path
    try {
        & cl /nologo /TC /Za /W4 /WX /DRV_NATIVE_TEST ('/I' + $scope.Path) ('/I' + $source) ('/I' + (Join-Path $sdk 'FCL')) (Join-Path $source 'recovery.c') (Join-Path $repo 'tests\rh850_reset_vector_recovery_tests.c') /Fe:recovery_tests.exe
        if ($LASTEXITCODE) { throw 'Recovery native model compile failed.' }
        if ($InjectFailure) { throw 'Intentional runner cleanup verification after EXE creation.' }
        $messages = & .\recovery_tests.exe
        if ($LASTEXITCODE) { throw 'Recovery native model failed.' }
        $result.Output = $messages -join "`n"
        Write-Host $result.Output
        $result.Result = 'PASS'
        $result.SourceHashes = @(Get-Item -LiteralPath (Join-Path $source 'recovery.c'), (Join-Path $source 'recovery.h'), (Join-Path $repo 'tests\rh850_reset_vector_recovery_tests.c') | Get-FileHash -Algorithm SHA256 | Select-Object Path, Hash)
    } finally { Pop-Location }
} catch {
    $result.Error = $_.Exception.Message
    throw
} finally {
    [IO.File]::WriteAllText((Join-Path $evidence 'result.json'), ($result | ConvertTo-Json -Depth 5))
    Remove-OwnedTestWorkspace $scope
}
