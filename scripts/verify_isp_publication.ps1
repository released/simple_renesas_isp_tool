# Exercise the selected production entry against owned local bare remotes.
param([switch]$IncludeRh850,[switch]$IncludeF24I2c,[switch]$IncludeF13I2c)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_reset.ps1')
. (Join-Path $PSScriptRoot 'github_publish_catalog.ps1')
. (Join-Path $PSScriptRoot 'github_publish_release.ps1')
$toolRoot=Split-Path -Parent $PSScriptRoot
$scope=New-OwnedTestWorkspace 'isp_publish' -ShortPath
$results=New-Object 'Collections.Generic.List[string]'
function Check($Value,[string]$Name) {
    if(!$Value) { throw $Name }
    $results.Add($Name)
    Write-Host "PASS: $Name"
}
function Write-TestFile([string]$Path,[string]$Text) {
    Assert-PublishTree $scope.Path $Path
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    [IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false)))
}
function Run-Entry([string[]]$Arguments,[int]$Exit,[string]$Pattern,[string]$InputText='') {
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\powershell.exe'
    $mode=@('-NoProfile')
    if(!$InputText) { $mode+='-NonInteractive' }
    $entryOptions=@()
    if($IncludeRh850) { $entryOptions+='-IncludeRh850' }
    if($IncludeF24I2c) { $entryOptions+='-IncludeF24I2c' }
    if($IncludeF13I2c) { $entryOptions+='-IncludeF13I2c' }
    $start.Arguments=($mode+@('-ExecutionPolicy','Bypass','-File',(Join-Path $fixtureTool 'scripts\push_github.ps1'))+$entryOptions+$Arguments | ForEach-Object {ConvertTo-PublishArgument $_}) -join ' '
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $start.RedirectStandardInput=[bool]$InputText
    $process=New-Object Diagnostics.Process
    $process.StartInfo=$start
    try {
        [void]$process.Start()
        if($InputText) { $process.StandardInput.WriteLine($InputText); $process.StandardInput.Close() }
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        # Ten-repository review/fetch/hash/stage checks launch many local Git
        # processes. Keep a harness bound separate from all device timeouts.
        if(!$process.WaitForExit(600000)) {
            $process.Kill();
            $process.WaitForExit();
            $failure=$stdout.Result+"`n"+$stderr.Result
            [IO.File]::WriteAllText((Join-Path $toolRoot 'tmp\internal\verification\isp_publication_timeout.txt'),$failure)
            throw 'Owned publication fixture timed out; captured output retained.'
        }
        $output=$stdout.Result+"`n"+$stderr.Result
        if($process.ExitCode -ne $Exit -or $output -notmatch $Pattern) { throw "Entry $($process.ExitCode), expected $Exit / $Pattern : $output" }
    } finally { $process.Dispose() }
}
try {
    $fixtureTool=Join-Path $scope.Path 'simple_renesas_isp_tool'
    [void][IO.Directory]::CreateDirectory((Join-Path $fixtureTool 'scripts'))
    foreach($name in @('push_github.ps1','workspace_paths.ps1','github_publish_git.ps1','github_publish_dependencies.ps1','github_publish_reset.ps1','github_publish_release.ps1','github_publish_initialize.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $fixtureTool ('scripts\'+$name))
    }
    $catalog=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'github_publish_catalog.ps1')).Replace('function Get-GithubPublishTargets','function Get-FixtureCatalog')
    $catalog+=@'

function Get-GithubPublishTargets {
    $all=@(Get-FixtureCatalog @args)
    foreach($target in $all) {
        $target.Root=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) $target.Name
        $target.Remote=$target.Root+'.git'
    }
    return $all
}
'@
    Write-TestFile (Join-Path $fixtureTool 'scripts\github_publish_catalog.ps1') $catalog
    $targets=@(Get-GithubPublishTargets $fixtureTool -IncludeRh850:$IncludeRh850 -IncludeF24I2c:$IncludeF24I2c -IncludeF13I2c:$IncludeF13I2c)
    $expectedCount=10; $expectedScope='renesas-isp-tool-host-can-10'
    if($IncludeRh850) { $expectedCount=16; $expectedScope='renesas-isp-tool-host-can-16' }
    if($IncludeF24I2c) {
        $expectedCount=13;$expectedScope='renesas-tool-host-rl78-can-f24-i2c-13'
        if($IncludeRh850) {$expectedCount=19;$expectedScope='renesas-isp-tool-host-can-f24-i2c-19'}
    }
    if($IncludeF13I2c) {
        $expectedCount+=3; $expectedScope='renesas-isp-tool-host-can-'+$(if($IncludeF24I2c){'f24-i2c-'}else{''})+'f13-i2c-'+$expectedCount
    }
    foreach($target in $targets) {
        $target.Root=Join-Path $scope.Path $target.Name
        $target.Remote=$target.Root+'.git'
        [void][IO.Directory]::CreateDirectory($target.Root)
        [void](Invoke-PublishGit $scope.Path @('init','--bare',$target.Remote))
        [void](Invoke-PublishGit $target.Root @('init','-b','main'))
        [void](Invoke-PublishGit $target.Root @('config','user.name','Synthetic ISP Publisher'))
        [void](Invoke-PublishGit $target.Root @('config','user.email','fixture@example.invalid'))
        Write-TestFile (Join-Path $target.Root '.gitignore') "tmp/`ngithub_push.local.psd1`nbuild/*.ini`nbuild/reports/`n"
        Write-TestFile (Join-Path $target.Root 'README.md') 'synthetic seed'
        foreach($relative in @($target.RequiredFiles | Where-Object {$_})) { Write-TestFile (Join-Path $target.Root $relative) 'synthetic EXE; never executed' }
        foreach($group in $target.Groups) {
            foreach($keep in $group.Keep) { Write-TestFile (Join-Path $target.Root ($group.Relative+'/'+$keep)) ('synthetic '+$keep) }
            Write-TestFile (Join-Path $target.Root ($group.Relative+'/diagnostic.map')) 'recoverable output'
        }
        if($target.ToolScope) {
            Write-TestFile (Join-Path $target.Root 'src\main.cpp') 'synthetic PC source'
            Write-TestFile (Join-Path $target.Root 'FirmwareUpgradeTool.vcxproj') '<Project><ItemGroup><ClCompile Include="src/main.cpp" /></ItemGroup></Project>'
            Write-TestFile (Join-Path $target.Root 'firmware/common/isp_device/isp_protocol.h') '// accepted shared wire definitions'

            Write-TestFile (Join-Path $target.Root 'firmware\common\bridge_uart.c') 'synthetic host source'
            Write-TestFile (Join-Path $target.Root 'firmware\common\isp_device\isp_device.c') 'previously tracked withheld baseline'
            Write-TestFile (Join-Path $target.Root 'docs\RH850_tracked.md') 'previously tracked withheld guide'
        }
        [void](Invoke-PublishGit $target.Root @('add','--all'))
        [void](Invoke-PublishGit $target.Root @('commit','-m','synthetic baseline'))
        [void](Invoke-PublishGit $target.Root @('remote','add','origin',$target.Remote))
        [void](Invoke-PublishGit $target.Root @('push','origin','main'))
        Write-TestFile (Join-Path $target.Root 'README.md') 'reviewed synthetic release'
        Write-TestFile (Join-Path $target.Root 'tmp\private.log') 'preserve'
    }
    $tool=$targets[-1]
    $quality='QualityReport(Synthetic,DefaultBuild).txt'
    $qualityRoot=$targets[0].Root
    Write-TestFile (Join-Path $qualityRoot $quality) 'private compiler paths; retain locally'
    [void](Invoke-PublishGit $qualityRoot @('add','--',$quality))
    [void](Invoke-PublishGit $qualityRoot @('commit','-m','synthetic tracked build log'))
    try { Assert-PublishLocalOnlyData $targets[0]; throw 'Expected tracked build log refusal.' }
    catch { Check ($_.Exception.Message -match 'Local-only data is tracked') 'tracked CS+ QualityReport refuses publication' }
    [void](Invoke-PublishGit $qualityRoot @('rm','--cached','--',$quality))
    try { Assert-PublishLocalOnlyData $targets[0]; throw 'Expected unignored build log refusal.' }
    catch { Check ($_.Exception.Message -match 'Local-only data is not ignored') 'unignored CS+ QualityReport refuses publication' }
    Add-Content -LiteralPath (Join-Path $qualityRoot '.gitignore') -Value '/QualityReport(*).txt' -Encoding UTF8
    Assert-PublishLocalOnlyData $targets[0]
    Assert-PublishStagedChanges $qualityRoot
    Check (Test-Path -LiteralPath (Join-Path $qualityRoot $quality)) 'explicit build log untracking preserves local bytes'
    Check (!(Test-PublishLocalOnlyPath 'docs/QualityReport(Synthetic).txt') -and !(Test-PublishLocalOnlyPath 'QualityReportSource.txt')) 'QualityReport rule is limited to generated root report names'
    Write-TestFile (Join-Path $fixtureTool 'src\main.cpp') '#include "../firmware/common/isp_device/isp_protocol.h"'
    Write-TestFile (Join-Path $fixtureTool 'docs\FIELD_GUIDE.md') 'reviewed Tool guide'
    Write-TestFile (Join-Path $fixtureTool 'build\FirmwareUpgradeTool.exe') 'reviewed synthetic EXE; never executed'
    Write-TestFile (Join-Path $fixtureTool 'build\runtime.ini') 'preserve settings'
    Write-TestFile (Join-Path $fixtureTool 'firmware\common\bridge_uart.c') 'reviewed host source'
    Write-TestFile (Join-Path $fixtureTool 'firmware\common\isp_device\isp_device.c') 'withheld local change'
    Write-TestFile (Join-Path $fixtureTool 'firmware\UnlistedFutureMcu\source.c') 'withheld unknown MCU'
    Write-TestFile (Join-Path $fixtureTool 'docs\RH850_tracked.md') 'withheld guide change'
    Write-TestFile (Join-Path $fixtureTool 'scripts\build_rh850.ps1') 'withheld dedicated build tool'

    $dependency=Join-Path $fixtureTool 'firmware/common/isp_device/isp_protocol.h'
    Write-TestFile $dependency '// unreviewed dependency change'
    try { [void](New-PublishPlan $targets); throw 'Expected dependency refusal.' }
    catch { Check ($_.Exception.Message -match 'Withheld Tool build dependency changed') 'changed withheld dependency refuses before archive/staging' }
    Write-TestFile $dependency '// accepted shared wire definitions'
    $source=Join-Path $fixtureTool 'src/main.cpp'
    $reviewedSource=[IO.File]::ReadAllText($source)
    Write-TestFile $source '#include "missing.h"'
    try { [void](New-PublishPlan $targets); throw 'Expected missing include refusal.' }
    catch { Check ($_.Exception.Message -match 'Missing quoted Tool include') 'missing quoted include refuses before archive/staging' }
    Write-TestFile $source $reviewedSource
    $plan=New-PublishPlan $targets
    $snapshot=Get-CanPublishSnapshot $plan -IncludeTool
    Check ($snapshot.Projects.Count -eq $expectedCount -and $snapshot.Scope -ceq $expectedScope) 'explicit repository inventory and schema'
    if($IncludeF24I2c) {
        Check ($snapshot.Schema -eq $(if($IncludeF13I2c){5}else{4})) 'selected I2C release owns its distinct receipt schema'
        $partial=@($plan | Where-Object Name -ne 'RL78_F24_ISP_Boot_loader_I2C_app')
        try {Assert-CanPublishScope $partial -IncludeTool;throw 'Missing I2C triplet refusal.'}
        catch {Check ($_.Exception.Message -match 'Partial triplets') 'partial F24 I2C triplet refused'}
        $old=$snapshot|ConvertTo-Json -Depth 10|ConvertFrom-Json
        $old.Schema=3;$old.BoardValidated=$true;$old.Evidence='synthetic fixture only'
        try {Assert-CanReleaseAcceptance $old $snapshot;throw 'Missing old schema refusal.'}
        catch {Check ($_.Exception.Message -match 'no matching acceptance') 'schema3 cannot authorize F24 I2C'}
        Check (!(Test-ToolPublicationPath 'firmware/common/isp_device/isp_i2c.c')) 'I2C triplet does not implicitly publish shared serial firmware'
    }
    if($IncludeRh850) {
        $partial=@($plan | Where-Object Name -ne 'Sample_Project_RH850_S1_CAN_Transfer')
        try { Assert-CanPublishScope $partial -IncludeTool; throw 'Missing partial scope refusal.' }
        catch { Check ($_.Exception.Message -match 'Partial triplets') 'partial RH850 triplet refused' }
        Check (Test-ToolPublicationPath 'docs/RH850_CAN_DEVICE_GUIDE_zh-TW.md') 'CAN-owned RH850 guide selected'
        Check (!(Test-ToolPublicationPath 'docs/RH850_ISP_DEVICE_DESIGN_zh-TW.md')) 'RH850 serial guide withheld'
        Check (Test-ToolPublicationPath 'firmware/common/rh850_can_v4/rh850_can_ports.c') 'CAN-owned driver mirror selected'
        Check (!(Test-ToolPublicationPath 'firmware/common/isp_rh850/isp_rh850_uart.c')) 'serial runtime remains withheld'
    }
    $selected=@($snapshot.Projects[-1].Files.Path)
    Check ($quality -cnotin $snapshot.Projects[0].Files.Path) 'private build log excluded from exact snapshot'
    Check (@($snapshot.Projects[-1].Dependencies | Where-Object { !$_.Selected -and $_.BaselineBlob }).Count -eq 1) 'snapshot binds unchanged withheld wire dependency without staging it'

    Check ('build/FirmwareUpgradeTool.exe' -cin $selected -and 'docs/FIELD_GUIDE.md' -cin $selected -and 'src/main.cpp' -cin $selected) 'Tool EXE, new document and PC source selected'
    Check ('firmware/common/bridge_uart.c' -cin $selected -and !($selected -match 'UnlistedFutureMcu|isp_device|RH850|build_rh850')) 'Host dependencies selected; unknown MCU and dedicated support withheld'
    Run-Entry @() 0 'PLAN ONLY'
    Check (!(Test-Path -LiteralPath (Join-Path $fixtureTool 'tmp\internal\github_publish'))) 'plan performs no archive, commit or push'
    Run-Entry @('-Execute','-Yes') 1 'board acceptance is required'
    $acceptance=$snapshot | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $acceptance.BoardValidated=$true; $acceptance.Evidence='Synthetic local fixture only; not hardware evidence.'
    $receipt=Join-Path $fixtureTool 'tmp\reviewed.json'
    Write-TestFile $receipt ($acceptance|ConvertTo-Json -Depth 10)
    Write-TestFile (Join-Path $fixtureTool 'docs\FIELD_GUIDE.md') 'same-status content drift'
    Run-Entry @('-Execute','-Yes','-AcceptanceFile',$receipt) 1 'no matching acceptance'
    Write-TestFile (Join-Path $fixtureTool 'docs\FIELD_GUIDE.md') 'reviewed Tool guide'
    $exe=Join-Path $fixtureTool 'build\FirmwareUpgradeTool.exe'
    $saved=Join-Path $scope.Path 'saved-exe'
    Assert-PublishTree $scope.Path $exe; Assert-PublishTree $scope.Path $saved
    Move-Item -LiteralPath $exe -Destination $saved
    Run-Entry @('-Execute','-Yes','-AcceptanceFile',$receipt) 1 'Missing/empty required delivery'
    Move-Item -LiteralPath $saved -Destination $exe
    Check (Test-Path -LiteralPath (Join-Path $plan[0].Root 'DefaultBuild\diagnostic.map')) 'missing EXE/content drift refuse before any pruning'
    Run-Entry @('-Execute','-Yes','-AcceptanceFile',$receipt) 0 "All $expectedCount remote main branches verified"
    foreach($target in $targets) {
        $head=(Invoke-PublishGit $target.Root @('rev-parse','HEAD')).Text
        Check ((Invoke-PublishGit $target.Root @('ls-remote','origin','refs/heads/main')).Text.Split("`t")[0] -ceq $head) ('verified local remote '+$target.Name)
        foreach($group in $target.Groups) {
            Check (@(Get-ChildItem -LiteralPath (Join-Path $target.Root $group.Relative)).Count -eq $group.Keep.Count) ('exact keeper set '+$target.Name+'/'+$group.Relative)
        }
    }
    Check ((Invoke-PublishGit $fixtureTool @('show','HEAD:build/FirmwareUpgradeTool.exe')).Text -ceq 'reviewed synthetic EXE; never executed') 'EXE reaches Tool remote without building or executing it'
    Check ((Invoke-PublishGit $fixtureTool @('show','HEAD:docs/FIELD_GUIDE.md')).Text -ceq 'reviewed Tool guide') 'Tool documents included by the default entry'
    Check ((Invoke-PublishGit $fixtureTool @('show','HEAD:firmware/common/bridge_uart.c')).Text -ceq 'reviewed host source') 'reviewed F24 host common source included'
    Check ((Invoke-PublishGit $fixtureTool @('show','HEAD:firmware/common/isp_device/isp_device.c')).Text -ceq 'previously tracked withheld baseline') 'withheld tracked source remains unchanged remotely'
    Check (([IO.File]::ReadAllText((Join-Path $fixtureTool 'firmware\common\isp_device\isp_device.c'))) -ceq 'withheld local change') 'withheld tracked source preserved locally'
    Check (!(Invoke-PublishGit $fixtureTool @('ls-tree','--name-only','HEAD','--','firmware/UnlistedFutureMcu/source.c')).Text) 'unlisted MCU source never staged'
    Check (([IO.File]::ReadAllText((Join-Path $fixtureTool 'build\runtime.ini'))) -ceq 'preserve settings') 'runtime INI preserved'
    Check (!(Invoke-PublishGit $qualityRoot @('ls-tree','--name-only','HEAD','--',$quality)).Text -and
        [IO.File]::ReadAllText((Join-Path $qualityRoot $quality)) -ceq 'private compiler paths; retain locally') 'published tree excludes build log while local file survives'
    if($IncludeF13I2c) {
        $partial=@($targets|Where-Object Name -ne 'RL78_F13_ISP_Boot_loader_I2C_app')
        try { Assert-CanPublishScope $partial -IncludeTool; throw 'Expected partial F13 refusal.' }
        catch { Check ($_.Exception.Message -match 'Partial triplets') 'partial F13 I2C triplet is refused' }
        $legacy=($acceptance|ConvertTo-Json -Depth 10|ConvertFrom-Json);$legacy.Schema=4
        try { Assert-CanReleaseAcceptance $legacy $snapshot; throw 'Expected schema4 refusal.' }
        catch { Check ($_.Exception.Message -match 'matching acceptance') 'legacy schema4 cannot authorize expanded F13 scope' }
    }
    $heads=@($targets|ForEach-Object{(Invoke-PublishGit $_.Root @('rev-parse','HEAD')).Text})
    Run-Entry @('-Execute') 0 "All $expectedCount remote main branches verified" 'PUSH'
    Check (@(Compare-Object $heads @($targets|ForEach-Object{(Invoke-PublishGit $_.Root @('rev-parse','HEAD')).Text})).Count -eq 0) 'matching receipt reuse and idempotent selected-repository repeat'
    $private='firmware/UnknownFutureFamily/private.c'
    Write-TestFile (Join-Path $fixtureTool $private) 'withheld history must not leak'
    [void](Invoke-PublishGit $fixtureTool @('add','--',$private))
    [void](Invoke-PublishGit $fixtureTool @('commit','-m','synthetic private addition'))
    [void](Invoke-PublishGit $fixtureTool @('rm','--cached','--',$private))
    [void](Invoke-PublishGit $fixtureTool @('commit','-m','synthetic private removal'))
    Write-TestFile (Join-Path $plan[0].Root 'DefaultBuild\diagnostic.map') 'must remain on refusal'
    Run-Entry @('-Execute','-Yes','-AcceptanceFile',$receipt) 1 'withheld content exists in outgoing Tool history'
    Check ((Invoke-PublishGit $fixtureTool @('ls-remote','origin','refs/heads/main')).Text.Split("`t")[0] -ceq $heads[-1]) 'withheld add/delete history rejected; remote unchanged'
    Check (Test-Path -LiteralPath (Join-Path $plan[0].Root 'DefaultBuild\diagnostic.map')) 'history refusal precedes first archive move'
} finally {
    $bytes=(Get-ChildItem -LiteralPath $scope.Path -Recurse -Force -File|Measure-Object Length -Sum).Sum
    Remove-OwnedTestWorkspace $scope
    $record=@{Cases=$results.Count;Passed=$results.ToArray();FixtureRemoved=!(Test-Path -LiteralPath $scope.Path);RetiredBytes=$bytes;HardwareRun=$false;GithubPush=$false}
    $output=Join-Path $toolRoot 'tmp\internal\verification\isp_publication_result.json'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $output))
    $record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $output -Encoding UTF8
}
Write-Host "$($results.Count) ISP publication checks PASS; owned local repositories retired."
