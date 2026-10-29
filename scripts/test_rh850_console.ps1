param([Parameter(Mandatory=$true)][string]$DeviceRoot,[string]$FormatterProject='', [string]$V4Project='', [ValidateSet(0,1)][int]$FullMode=0)
$ErrorActionPreference='Stop'
$tool=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'device_project_catalog.ps1')
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
function Get-ConsoleFunction([string]$Text,[string]$Name){
    $match=[regex]::Match($Text,'(?m)^(?:static )?(?:void|int) '+[regex]::Escape($Name)+'\([^\r\n]*\)\r?\n\{')
    if(!$match.Success){throw ('Missing actual console function: '+$Name)}
    $position=$Text.IndexOf('{',$match.Index)
    $depth=1
    while($depth -gt 0){$position++;if($Text[$position] -eq '{'){$depth++}elseif($Text[$position] -eq '}'){$depth--}}
    return $Text.Substring($match.Index,$position-$match.Index+1)
}
$projects=@(Get-DeviceProjectCatalog $DeviceRoot|Where-Object {$_.Name -like 'Sample_Project_RH850*'})
if($projects.Count -ne 15){throw 'Expected15 independent RH850 ISP/Transfer projects.'}
$reference=Join-Path $tool 'firmware\common\isp_rh850'
$canonical=[IO.File]::ReadAllText((Join-Path $reference 'retarget.c')).Replace("`r`n","`n")
$canonicalHeader=[IO.File]::ReadAllText((Join-Path $reference 'retarget.h')).Replace("`r`n","`n")
$send=''
foreach($project in $projects){
    $source=[IO.File]::ReadAllText((Join-Path $project.Project 'custom_func.c'))
    if($source -match '\b(?:Device_Log|Device_LogHex|UartChar|LogByte|LogCounter)\s*\('){throw ('Duplicate console API: '+$project.Name)}
    if($source -notmatch '#include "retarget.h"'){throw 'Missing local console declaration.'}
    foreach($file in @('retarget.c','retarget.h','retarget_full.inc')){
        $expected=[IO.File]::ReadAllText((Join-Path $reference $file)).Replace("`r`n","`n")
        if([IO.File]::ReadAllText((Join-Path $project.Project $file)).Replace("`r`n","`n") -cne $expected){throw ('Local formatter drift: '+$project.Name)}
    }
    [xml]$xml=[IO.File]::ReadAllText((Join-Path $project.Project ($project.Name+'.mtpj')))
    foreach($file in @('retarget.c','retarget.h')){
        if(@($xml.SelectNodes('//Instance[Type="File"]')|Where-Object {$_.RelativePath -ceq $file}).Count -ne 1){throw ('Missing/duplicate CS+ console binding: '+$project.Name)}
    }
    $body=(Get-ConsoleFunction $source SendChar).Replace("`r`n","`n")
    if($send -and $send -cne $body){throw 'UART console guard differs between projects.'}
    $send=$body
}
$classic=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_CAN_Transfer'
$fd=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_CAN_FD_Transfer'
$boot=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_Boot_loader_CAN'
$serial=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_ISP_Boot_loader_UART_app'
if(!$FormatterProject){$FormatterProject=$classic}
$FormatterProject=(Resolve-Path -LiteralPath $FormatterProject).Path
$classicTrace=(Get-ConsoleFunction ([IO.File]::ReadAllText((Join-Path $classic 'custom_func.c'))) Trace) -replace '\bTrace\(','ClassicalTrace('
$fdTrace=(Get-ConsoleFunction ([IO.File]::ReadAllText((Join-Path $fd 'custom_func.c'))) Trace) -replace '\bTrace\(','FdTrace('
$bootSource=[IO.File]::ReadAllText((Join-Path $boot 'custom_func.c'))
$serialSource=[IO.File]::ReadAllText((Join-Path $serial 'custom_func.c'))
$reset=[regex]::Match($bootSource,'(?m)^\s*tiny_printf\("reset cause:[^\r\n]+').Value.Trim()
$counter=[regex]::Match($bootSource,'(?m)^\s*tiny_printf\("log\(timer\)[^\r\n]+').Value.Trim()
$serialCounter=[regex]::Match($serialSource,'(?m)^\s*tiny_printf\("log\(timer\)[^\r\n]+').Value.Trim()
if(!$reset -or !$counter -or $counter -cne $serialCounter){throw 'Actual reset/counter call sites changed;review expected text.'}
$v4Send=''
if($V4Project){
    $V4Project=(Resolve-Path -LiteralPath $V4Project).Path
    foreach($file in @('retarget.c','retarget.h')){
        if((Get-FileHash -LiteralPath (Join-Path $V4Project $file)).Hash -cne (Get-FileHash -LiteralPath (Join-Path $FormatterProject $file)).Hash){throw 'v4/ISP formatter mismatch.'}
    }
    $v4Source=[IO.File]::ReadAllText((Join-Path $V4Project 'custom_func.c'))
    $v4Send=(Get-ConsoleFunction $v4Source SendChar) -replace '\bSendChar\(','V4SendChar('
}
$scope=New-OwnedTestWorkspace 'rh850_console'
try {
    $include=$send+"`n"+$classicTrace+"`n"+$fdTrace+"`n"+
        'static void PrintActualReset(unsigned long g_reset_cause) {'+$reset+"}`n"+
        'static unsigned long PrintActualCounter(unsigned long g_custom_counter) {'+$counter+'return g_custom_counter;}'
    $include+="`n"+$v4Send
    [IO.File]::WriteAllText((Join-Path $scope.Path 'actual_console.inc'),$include,(New-Object Text.UTF8Encoding($false)))
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs){throw 'Existing MSVC required for native verification only.'}
    $lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=x86 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    Push-Location -LiteralPath $scope.Path
    try {
        $hal=Join-Path $classic 'hal\isp'
        # The full original formatter uses C99; the protocol remains C90.
        # Preserve the original uint32 pointer typedef and bounded char digit
        # assignments. MSVC warns on those legacy forms; the x86 ABI is asserted
        # below and all other warning categories remain errors.
        # Rename only the native symbol to avoid replacing the host CRT putchar.
        & cl /nologo /TC /std:c11 /W4 /WX /wd4244 ('/DTINY_PRINTF_FULL='+$FullMode) /Dputchar=RetargetPutchar /c ('/I'+$FormatterProject) (Join-Path $FormatterProject 'retarget.c')
        if($LASTEXITCODE){throw 'Actual full formatter compile failed.'}
        & cl /nologo /TC /Za /W4 /WX /c ('/I'+$classic) ('/I'+$hal) ('/I'+(Join-Path $classic 'hal\device')) (Join-Path $hal 'can_transfer_protocol.c')
        if($LASTEXITCODE){throw 'Actual C90 protocol compile failed.'}
        $referenceSource=Join-Path $DeviceRoot 'Sample_Project_RH850_S1_UART_printf\retarget.c'
        $originalFolder=Join-Path $scope.Path reference
        [IO.Directory]::CreateDirectory($originalFolder)|Out-Null
        Copy-Item -LiteralPath $referenceSource -Destination (Join-Path $originalFolder 'reference_retarget.c')
        Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $referenceSource) 'retarget.h') -Destination (Join-Path $originalFolder 'retarget.h')
        & cl /nologo /TC /std:c11 /W3 /c /Dtiny_printf=OriginalTinyPrintf /Dputchar=OriginalPutchar /DSendChar=OriginalSendChar (Join-Path $originalFolder 'reference_retarget.c')
        if($LASTEXITCODE){throw 'Original formatter comparison compile failed.'}
        foreach($trace in @(0,1)){
            $extra=@();if($v4Send){$extra+='/DHAS_V4_CONSOLE'}
            & cl /nologo /std:c++17 /EHsc /W4 /WX @extra ('/DTINY_PRINTF_FULL='+$FullMode) ('/DCAN_TRANSFER_UART_TRACE='+$trace) ('/I'+$FormatterProject) ('/I'+$scope.Path) (Join-Path $tool 'tests\rh850_console_tests.cpp') retarget.obj reference_retarget.obj can_transfer_protocol.obj /Fe:rh850_console_tests.exe
            if($LASTEXITCODE){throw 'Console capture test compile failed.'}
            $stdout=Join-Path $scope.Path ('run_'+$trace+'.log')
            $stderr=$stdout+'.stderr'
            $process=New-Object Diagnostics.Process
            $process.StartInfo.FileName=Join-Path $scope.Path 'rh850_console_tests.exe'
            $process.StartInfo.WorkingDirectory=$scope.Path
            $process.StartInfo.UseShellExecute=$false
            $process.StartInfo.CreateNoWindow=$true
            $process.StartInfo.RedirectStandardOutput=$true
            $process.StartInfo.RedirectStandardError=$true
            if(!$process.Start()){throw 'Owned test process did not start.'}
            $outputTask=$process.StandardOutput.ReadToEndAsync()
            $errorTask=$process.StandardError.ReadToEndAsync()
            try {
                if(!$process.WaitForExit(30000)){
                    $process.Kill();$process.WaitForExit()
                    throw 'Owned console edge test timed out; possible non-finite normalization loop.'
                }
                [IO.File]::WriteAllText($stdout,$outputTask.GetAwaiter().GetResult())
                [IO.File]::WriteAllText($stderr,$errorTask.GetAwaiter().GetResult())
                Get-Content -LiteralPath $stdout | Write-Host
                Get-Content -LiteralPath $stderr | Write-Host
                if($null -eq $process.ExitCode -or $process.ExitCode -ne 0){throw 'Actual console output/guard test failed.'}
            }finally{$process.Dispose()}
        }
    }finally{Pop-Location}
    Write-Host ('PASS15 local console bindings;mode='+$FullMode+';foreground calls,traces on/off and bounded checked UART hook. Hardware NOT RUN.')
}finally{Remove-OwnedTestWorkspace $scope}
