param(
    [string]$AppProject = (Join-Path $PSScriptRoot '..\firmware\RL78_F24_Bridge_app'),
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'f24_app_contract.ps1')
$contract = Get-F24AppContract $AppProject
$identity = @"
/* Generated from this app's app_image.json. Do not edit. */
#ifndef F24_APP_IDENTITY_GENERATED_H
#define F24_APP_IDENTITY_GENERATED_H
#define F24_APP_IMAGE_VERSION (0x$($contract.Version.ToString('X8'))UL)
#define F24_APP_IMAGE_BUILD ($($contract.Build)UL)
#endif
"@ + "`r`n"
$outputs = @{
    'bridge_vectors.asm' = Get-F24AppVectorText $contract
    'f24_app_identity.generated.h' = $identity.Replace("`r`n","`n").Replace("`n","`r`n")
}
# Validate the entire input before publishing either generated file.
foreach ($name in $outputs.Keys) {
    $path = Join-Path $contract.Root $name
    $same = (Test-Path -LiteralPath $path) -and [IO.File]::ReadAllText($path) -ceq $outputs[$name]
    if ($CheckOnly -and !$same) { throw "Stale app-local generated file: $path" }
    if (!$CheckOnly -and !$same) { [IO.File]::WriteAllText($path,$outputs[$name],(New-Object Text.UTF8Encoding($false))) }
}
Write-Host "PASS: $($contract.Name) app-local IRQ/identity $(if($CheckOnly){'verified'}else{'prepared'}); boot untouched."
