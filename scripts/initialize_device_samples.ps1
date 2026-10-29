param(
    [Parameter(Mandatory=$true)][string]$DeviceRoot,
    [string]$ReferenceRoot='',
    [ValidateSet('F13','F24','G16','G22','RH850')][string[]]$Mcu=@('F13','F24','G16','G22','RH850'),
    [ValidateSet('CAN','CAN_FD','UART','I2C','Xmodem')][string[]]$Bus=@('UART','I2C','Xmodem'),
    [ValidateSet('boot','app','transfer')][string[]]$Role=@('boot','app','transfer'),
    [hashtable]$VendorOverrides=@{},[switch]$CheckOnly
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'device_project_catalog.ps1')
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')

function Assert-PlainPath([string]$Path) {
    $probe=[IO.Path]::GetFullPath($Path)
    while($probe) {
        if((Test-Path -LiteralPath $probe) -and
           ((Get-Item -LiteralPath $probe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw ('Reparse template path refused: '+$probe)
        }
        $probe=[IO.Path]::GetDirectoryName($probe)
    }
}

function Copy-TemplateFiles([string]$Source,[string]$Destination,[switch]$ProjectFiles) {
    Assert-PlainPath $Source;Assert-PlainPath $Destination
    $prefix=(Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
    $items=@(Get-ChildItem -LiteralPath $prefix -Recurse -Force)
    if($items|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}) {
        throw 'Reparse template descendant refused before copy.'
    }
    foreach($file in $items) {
        if($file.PSIsContainer){continue}
        $relative=$file.FullName.Substring($prefix.Length+1)
        if($ProjectFiles -and ($relative -match '^(DefaultBuild|tmp|test_bins|build|\.git|\.vs|RSCANconf_default)(\\|$)' -or
            $file.Name -like 'QualityReport*' -or $file.Extension -in @('.rcpe','.mtud','.log','.obj','.pdb','.pyc'))) {continue}
        $target=[IO.Path]::GetFullPath((Join-Path $Destination $relative))
        if(!$target.StartsWith([IO.Path]::GetFullPath($Destination).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Template output escaped the selected destination.'
        }
        Assert-PlainPath $target
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
    }
}

$destination=[IO.Path]::GetFullPath($DeviceRoot).TrimEnd('\')
Assert-PlainPath $destination
$names=@()
foreach($family in $Mcu){foreach($interface in $Bus) {
    if($interface -in @('CAN','CAN_FD') -and ($family -in @('G16','G22') -or ($family -eq 'F13' -and $interface -eq 'CAN_FD'))) {continue}
    $prefix=if($family -eq 'RH850'){'Sample_Project_RH850_S1'}else{'RL78_'+$family}
    $boot=$prefix+'_'+$(if($interface -in @('UART','I2C','Xmodem')){'ISP_'}else{''})+'Boot_loader_'+$interface
    foreach($kind in $Role) {
        $names+=switch($kind){
            boot {$boot}
            app {$boot+'_app'}
            transfer {$prefix+'_'+$(if($interface -eq 'I2C'){'I2C_Slave'}else{$interface})+'_Transfer'}
        }
    }
}}
if(!$names.Count){throw 'No supported device sample selected.'}
if($CheckOnly) {
    foreach($item in Get-DeviceProjectCatalog $destination $names){Assert-DeviceLocalBindings $item.Project}
    Write-Host ('PASS '+$names.Count+' independent device projects; no source changes.')
    return
}
if(!$ReferenceRoot){throw 'Specify -ReferenceRoot containing the current independent device golden samples.'}
$reference=(Resolve-Path -LiteralPath $ReferenceRoot).Path.TrimEnd('\')
Assert-PlainPath $reference
$sources=@(Get-DeviceProjectCatalog $reference $names)
foreach($override in $VendorOverrides.Values) {
    foreach($key in @('Library','Header')) {
        if($override[$key]) {
            Assert-PlainPath $override[$key]
            if(!(Test-Path -LiteralPath $override[$key])){throw ('Missing vendor '+$key+' input.')}
        }
    }
}
# Preflight the complete selection before creating any destination.
foreach($item in $sources) {
    Assert-DeviceLocalBindings $item.Project
    if(Test-Path -LiteralPath (Join-Path $destination $item.Name)) {
        throw ('Existing project is never overwritten: '+$item.Name)
    }
}
foreach($item in $sources) {
    $target=Join-Path $destination $item.Name
    Copy-TemplateFiles $item.Project $target -ProjectFiles
    $family=if($item.Name -match '^RL78_(F13|F24|G16|G22)_'){$Matches[1]}else{'RH850'}
    if($VendorOverrides.ContainsKey($family)) {
        $override=$VendorOverrides[$family]
        if($override.Library) {
            $libraryName=if($family -eq 'G16'){'RFSPRL78T01'}elseif($family -eq 'G22'){'RFDRL78T01'}else{'RFDRL78T02'}
            Copy-TemplateFiles $override.Library (Join-Path $target $libraryName)
        }
        if($override.Header) {
            Assert-PlainPath $override.Header
            Copy-Item -LiteralPath $override.Header -Destination (Join-Path $target 'iodefine.h') -Force
        }
    }
    Assert-DeviceLocalBindings $target
    Write-Host ('Created independent '+$item.Role+' project: '+$item.Name)
}
Write-Host 'Build each project using its local build.ps1. No MCU/COM/programming or publication performed.'
