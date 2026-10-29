param([Parameter(Mandatory=$true)][string]$DeviceRoot,[string[]]$ProjectName=@())
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'device_project_catalog.ps1')
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
$count=0
foreach($item in Get-DeviceProjectCatalog $DeviceRoot $ProjectName) {
    $root=$item.Project
    Assert-DeviceLocalBindings $root
    foreach($directory in @('common','can_v4','isp')) {
        if(Test-Path -LiteralPath (Join-Path $root $directory)) {
            throw ('Obsolete project module directory: '+$item.Name+'/'+$directory)
        }
    }
    $source=[IO.File]::ReadAllText((Join-Path $root 'custom_func.c'))
    if($source -notmatch 'void\s+hardware_init\s*\(void\)' -or $source -notmatch 'void\s+loop\s*\(void\)') {
        throw ('Missing local user callbacks: '+$item.Name)
    }
    [xml]$xml=[IO.File]::ReadAllText((Join-Path $root ($item.Name+'.mtpj')))
    foreach($key in @('COptionI-0','AsmOptionI-0','AOptionI-0','COptionPreinclude-0')) {
        $node=$xml.SelectSingleNode('//'+$key)
        if(!$node){continue}
        foreach($relative in $node.InnerText -split '\r?\n') {
            if(!$relative.Trim()){continue}
            if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)') {
                throw ('Non-local compiler input: '+$item.Name+' '+$relative)
            }
        }
    }
    foreach($node in $xml.SelectNodes('//Instance[Type="File"]')) {
        $relative=[string]$node.RelativePath
        if([IO.Path]::GetExtension($relative) -notin @('.c','.h','.asm','.inc')){continue}
        if([IO.Path]::GetFileName($relative) -in @('device_runtime.c','isp_rh850_runtime.c','isp_rl78_runtime.c','isp_runtime.c')) {
            throw ('Separate orchestration source: '+$relative)
        }
        $body=[IO.File]::ReadAllText((Join-Path $root $relative))
        if($body -match '(?m)^\s*#\s*include\s+"(?:[A-Za-z]:|(?:[^"\r\n]*[\\/])?\.\.[\\/])') {
            throw ('Parent/absolute include in '+$item.Name+'/'+$relative)
        }
    }
    $count++
}
Write-Host ('PASS '+$count+' project-local HAL/source/config/build bindings; no sibling source dependency.')
