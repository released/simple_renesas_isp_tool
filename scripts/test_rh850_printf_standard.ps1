param([string]$FormatterProject='', [switch]$Binary32)
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
if(!$FormatterProject){$FormatterProject=Join-Path $tool 'firmware\common\isp_rh850'}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace rh850_printf_standard
try {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs){throw 'Installed native compiler required for developer verification.'}
    $lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=x86 >nul && set')
    if($LASTEXITCODE){throw 'Native environment failed.'}
    foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    Push-Location -LiteralPath $scope.Path
    try {
        $compileRoot=$FormatterProject
        $extra=@()
        if($Binary32){
            # Emulate only the IEEE reader/float constants on the native ABI;
            # all decimal/hex/parser/output production code remains identical.
            # Real CC-RH ABI/type compilation is a separate target check.
            $compileRoot=Join-Path $scope.Path binary32
            [IO.Directory]::CreateDirectory($compileRoot)|Out-Null
            foreach($file in @('retarget.c','retarget.h','retarget_config.h','retarget_full.inc')){
                Copy-Item -LiteralPath (Join-Path $FormatterProject $file) -Destination (Join-Path $compileRoot $file)
            }
            $path=Join-Path $compileRoot 'retarget_full.inc'
            $text=[IO.File]::ReadAllText($path)
            $start=$text.IndexOf('static fp_binary fp_inspect(double value)')
            $end=$text.IndexOf('static void fp_multiply',$start)
            $reader=$text.Substring($start,$end-$start)
            $reader=$reader.Replace('fp_binary parts;','float encoded = (float)value;'+"`n    fp_binary parts;").Replace('&value','&encoded').Replace('sizeof(value)','sizeof(encoded)')
            [IO.File]::WriteAllText($path,$text.Substring(0,$start)+$reader+$text.Substring($end))
            [IO.File]::WriteAllText((Join-Path $compileRoot 'float.h'),"#define FLT_RADIX 2`n#define DBL_MANT_DIG 24`n#define DBL_MAX_EXP 128`n#define DBL_MIN_EXP (-125)`n#define DBL_MAX_10_EXP 38`n#define LDBL_MANT_DIG 24`n#define LDBL_MAX_EXP 128`n")
            $extra+='/DTINY_PRINTF_BINARY32_TEST'
        }
        & cl /nologo /TC /std:c11 /W4 /WX /wd4244 /Dputchar=RetargetPutchar /c ('/I'+$compileRoot) (Join-Path $compileRoot 'retarget.c')
        if($LASTEXITCODE){throw 'Full formatter compile failed.'}
        & cl /nologo /std:c++17 /EHsc /W4 /WX @extra ('/I'+$FormatterProject) (Join-Path $tool 'tests\rh850_printf_standard_tests.cpp') retarget.obj /Fe:standard_tests.exe
        if($LASTEXITCODE){throw 'Standard differential compile failed.'}
        & .\standard_tests.exe
        if($LASTEXITCODE){throw 'Standard differential tests failed.'}
    } finally {Pop-Location}
} finally {Remove-OwnedTestWorkspace $scope}
