param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$name='PUBLIC_CONTENT_GUIDELINES_zh-TW.md'
$source=[IO.File]::ReadAllText((Join-Path $WebRoot $name))
$prefix="<!-- Generated from decision-center rules/$name through /web. Edit the center source, not this copy. -->`n"
$legacyPrefix="<!-- Generated from /web/$name. Edit the common root source, not this copy. -->`n"
$expected=$prefix+$source
$target=Join-Path $repo ('docs\'+$name)
if(!$CheckOnly -and (Test-Path -LiteralPath $target)) {
    $current=[IO.File]::ReadAllText($target).Replace("`r`n","`n")
    $known=($current -ceq $expected.Replace("`r`n","`n") -or $current -ceq ($legacyPrefix+$source).Replace("`r`n","`n"))
    $statePath=Join-Path $WebRoot '.public-guidelines.local.json'
    if(!$known -and (Test-Path -LiteralPath $statePath)) {
        $state=[IO.File]::ReadAllText($statePath)|ConvertFrom-Json
        $body=if($current.StartsWith($prefix)){$current.Substring($prefix.Length)}elseif($current.StartsWith($legacyPrefix)){$current.Substring($legacyPrefix.Length)}else{''}
        $sha=[Security.Cryptography.SHA256]::Create()
        try {$bodyHash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($body)))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
        $known=($body -and $state.PSObject.Properties.Name -contains 'previous_source_hash' -and $bodyHash -ceq $state.previous_source_hash)
    }
    if(!$known) { throw 'Unmanaged Tool guideline edits preserved; reconcile the copy before synchronization.' }
}
if($CheckOnly -and (!(Test-Path -LiteralPath $target) -or
    [IO.File]::ReadAllText($target).Replace("`r`n","`n") -cne $expected.Replace("`r`n","`n"))) {
    throw 'Tool documentation copy differs from the common /web guidelines.'
}
& (Join-Path $WebRoot 'scripts\sync_public_content_guidelines.ps1') `
    -Project @('slide_boot_loader_for_RL78','slide_boot_loader_for_RH850','slide_simple_renesas_isp_tool') -CheckOnly:$CheckOnly
if(!$CheckOnly) { $source=[IO.File]::ReadAllText((Join-Path $WebRoot $name));$expected=$prefix+$source }
if(!$CheckOnly -and (!(Test-Path -LiteralPath $target) -or [IO.File]::ReadAllText($target) -cne $expected)) {
    [IO.File]::WriteAllText($target,$expected,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: Tool and slide copies use the common /web guidelines.'
