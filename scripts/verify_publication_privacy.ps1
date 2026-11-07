param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'github_publish_git.ps1')
. (Join-Path $PSScriptRoot 'github_publish_catalog.ps1')
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$scope=New-OwnedTestWorkspace 'publication_privacy'
$checks=0
try {
    foreach($path in @('AGENTS.md','HANDOFF.md','CODEX_HANDOFF.md','SELF_CHECK.md','MANUAL_CLEANUP.md','DEVELOPMENT_CHECKLIST.md','docs/verification/result.json','docs/development/notes.md')) {
        if(!(Test-PublishLocalOnlyPath $path) -or (Test-ToolPublicationPath $path)) { throw "Private publication path allowed: $path" }
        $checks++
    }
    foreach($path in @(
        'README.md',
        'docs/HOST_LINK_PROTOCOL_V2.md',
        'src/main_frame.cpp',
        'tests/can_client_tests.cpp',
        'docs/RH850_RESET_VECTOR_RECOVERY_zh-TW.md',
        'firmware/diagnostics/rh850_reset_vector/entry.asm',
        'firmware/diagnostics/rh850_reset_vector/fcl_cfg.h',
        'firmware/diagnostics/rh850_reset_vector/platform.c',
        'firmware/diagnostics/rh850_reset_vector/recovery.c',
        'firmware/diagnostics/rh850_reset_vector/recovery.h',
        'scripts/build_rh850_reset_vector_recovery.ps1',
        'scripts/test_rh850_reset_vector_recovery.ps1',
        'tests/rh850_reset_vector_recovery_tests.c'
    )) {
        if((Test-PublishLocalOnlyPath $path) -or !(Test-ToolPublicationPath $path)) { throw "Public source refused: $path" }
        $checks++
    }
    foreach($path in @(
        'firmware/diagnostics/unverified_helper.c',
        'docs/RH850_UNVERIFIED_DIAGNOSTIC_zh-TW.md',
        'scripts/test_rh850_unverified_diagnostic.ps1'
    )) {
        if(Test-ToolPublicationPath $path) { throw "Unreviewed diagnostic path allowed: $path" }
        $checks++
    }
    $repo=Join-Path $scope.Path 'repo'
    [void][IO.Directory]::CreateDirectory($repo)
    [void](Invoke-PublishGit $repo @('init','-b','main'))
    [IO.File]::WriteAllText((Join-Path $repo 'HANDOFF.md'),'Synthetic private engineering record')
    [void](Invoke-PublishGit $repo @('add','--','HANDOFF.md'))
    $before=(Get-FileHash -LiteralPath (Join-Path $repo '.git/index')).Hash
    $blocked=$false
    try { Assert-PublishLocalOnlyData @{Root=$repo} } catch {
        if($_.Exception.Message -notmatch 'Local-only data is tracked') { throw }
        $blocked=$true
    }
    if(!$blocked -or (Get-FileHash -LiteralPath (Join-Path $repo '.git/index')).Hash -cne $before) { throw 'Tracked-private guard did not preserve index' }
    if(!(Test-Path -LiteralPath (Join-Path $repo 'HANDOFF.md'))) { throw 'Private local file was removed' }
    $checks++
    Write-Host "PASS: $checks publication privacy checks; tracked-private rejection precedes publication and preserves local content/index."
} finally {
    Remove-OwnedTestWorkspace $scope
    Write-Host ('Owned fixture absent: '+!(Test-Path -LiteralPath $scope.Path))
}
