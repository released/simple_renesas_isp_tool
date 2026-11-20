param([ValidateSet('Release','Debug')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$hub=Join-Path (Split-Path -Parent $repo) '#READ_ME_FIRST#'
. (Join-Path $hub 'scripts/invoke_hub_test.ps1')
$case=Join-Path $repo 'tmp/internal/verification/pc_session_contract_20261120'
$output=''
$expected=@('worker_contract.exe','worker_contract.pdb','worker_responsiveness_tests.obj','log_view_tests.obj','stdout.log','stderr.log','FirmwareUpgradeTool.ini','ui_large.bin')
$result=Invoke-HubOwnedTest -ProjectRoot $repo -Purpose ('PC worker/session contract actual MFC '+$Configuration+' without hardware') -MaxBytes 96MB -Action {
    param($run)
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(!$vs){throw 'Installed MSVC/MFC required.'}
    $lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7/Tools/VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
    if($LASTEXITCODE){throw 'MSVC environment failed.'}
    foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    function Native([string]$Program,[string[]]$Arguments){
        $start=New-Object Diagnostics.ProcessStartInfo
        $start.FileName=(Get-Command $Program -CommandType Application|Select-Object -First 1).Source
        $start.Arguments=($Arguments|ForEach-Object {'"'+$_.Replace('"','\"')+'"'}) -join ' '
        $start.WorkingDirectory=$run.Work;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
        $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
        $p=New-Object Diagnostics.Process;$p.StartInfo=$start
        [void]$p.Start();$stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync()
        if(!$p.HasExited){Register-HubTestProcess -Run $run -ProcessId $p.Id}
        $p.WaitForExit();$code=$p.ExitCode;$p.Dispose()
        [IO.File]::WriteAllText((Join-Path $run.Work 'stdout.log'),$stdout.Result)
        [IO.File]::WriteAllText((Join-Path $run.Work 'stderr.log'),$stderr.Result)
        $script:output=$stdout.Result+$stderr.Result
        if($code){throw ($Program+' failed: '+$script:output)}
    }
    $objects=Join-Path $repo ('tmp/internal/build/obj/x64/'+$Configuration)
    $inputs=@(Get-ChildItem -LiteralPath $objects -Filter '*.obj'|Where-Object {$_.Name -notin @('app.obj','main.obj')}|ForEach-Object FullName)
    $resources=@(Get-ChildItem -LiteralPath $objects -Filter '*.res'|ForEach-Object FullName)
    if($inputs.Count -lt 30){throw 'Build the affected Tool configuration first.'}
    $flags=@('/nologo','/std:c++17','/W4','/WX','/EHsc','/D_UNICODE','/DUNICODE','/D_AFX_NO_MFC_CONTROLS_IN_DIALOGS')
    if($Configuration -eq 'Debug'){$flags+=@('/MTd','/D_DEBUG')}else{$flags+=@('/MT','/DNDEBUG')}
    Native 'cl.exe' ($flags+@((Join-Path $repo 'tests/worker_responsiveness_tests.cpp'),(Join-Path $repo 'tests/log_view_tests.cpp'))+
        $inputs+$resources+@('/Fe:worker_contract.exe','/link','/LTCG','/INCREMENTAL:NO','/DEBUG','/PDB:worker_contract.pdb','/SUBSYSTEM:CONSOLE','setupapi.lib','cfgmgr32.lib','comctl32.lib'))
    Native (Join-Path $run.Work 'worker_contract.exe') @()
    Write-Host $script:output
} -CollectArtifacts {
    param($run)
    foreach($p in Get-ChildItem -LiteralPath $run.Work -File){
        if($p.Name -notin $expected -and $p.Name -notmatch '^firmware_upgrade_tool_[0-9_]+\.log$'){throw ('Unknown fixture output preserved: '+$p.Name)}
        Register-HubArtifact -Run $run -Path $p.FullName -Producer 'declared compiler and isolated actual-frame fixture output'
    }
}
[void][IO.Directory]::CreateDirectory($case)
[ordered]@{Success=$result.Success;Outcome=$result.Outcome;CleanupRemoved=$result.CleanupRemoved;Errors=$result.Errors;Remaining=$result.Remaining;Configuration=$Configuration;Output=$script:output;Hardware=$false}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $case ($Configuration.ToLowerInvariant()+'_contract_result.json')) -Encoding UTF8
if(!$result.Success){throw 'Worker/session contract test or owned retirement failed.'}
