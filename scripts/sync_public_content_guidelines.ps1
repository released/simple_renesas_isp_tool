param([string]$WebRoot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
$name='PUBLIC_CONTENT_GUIDELINES_zh-TW.md'
$source=[IO.File]::ReadAllText((Join-Path $WebRoot $name))
$expected="<!-- Generated from /web/$name. Edit the common root source, not this copy. -->`n"+$source
$target=Join-Path $repo ('docs\'+$name)
if($CheckOnly -and (!(Test-Path -LiteralPath $target) -or
    [IO.File]::ReadAllText($target).Replace("`r`n","`n") -cne $expected.Replace("`r`n","`n"))) {
    throw 'Tool documentation copy differs from the common /web guidelines.'
}
& (Join-Path $WebRoot 'scripts\sync_public_content_guidelines.ps1') `
    -Project @('slide_boot_loader_for_RL78','slide_isp_tool_for_RL78') -CheckOnly:$CheckOnly
if(!$CheckOnly -and (!(Test-Path -LiteralPath $target) -or [IO.File]::ReadAllText($target) -cne $expected)) {
    [IO.File]::WriteAllText($target,$expected,(New-Object Text.UTF8Encoding($false)))
}
Write-Host 'PASS: Tool and slide copies use the common /web guidelines.'
