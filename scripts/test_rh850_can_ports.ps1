param([Parameter(Mandatory=$true)][string]$BootProject)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$common=Join-Path (Resolve-Path -LiteralPath $BootProject).Path 'common'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Existing Visual Studio C/C++ tools are required for models.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$out=Join-Path $repo 'tmp\internal\tests\rh850_can_ports'
New-Item -ItemType Directory -Path $out -Force | Out-Null
Push-Location -LiteralPath $out
try {
    for($mask=1;$mask -le 63;$mask++){
        $defs=@()
        for($ch=0;$ch -lt 6;$ch++){$defs+='/DENABLE_PORT_CAN'+$ch+'='+[int](($mask -band (1 -shl $ch)) -ne 0)}
        $ErrorActionPreference='Continue'
        & cl /nologo /TC /EP @defs (Join-Path $common 'can_port_config.h') 2>&1 | Out-Null
        $ErrorActionPreference='Stop'
        if($LASTEXITCODE){throw "Valid port configuration rejected: $mask"}
    }
    foreach($bad in @(0,-1,2)){
        $defs=@(('/DENABLE_PORT_CAN0='+$bad),'/DENABLE_PORT_CAN1=0','/DENABLE_PORT_CAN2=0','/DENABLE_PORT_CAN3=0','/DENABLE_PORT_CAN4=0','/DENABLE_PORT_CAN5=0')
        $ErrorActionPreference='Continue'
        & cl /nologo /TC /EP @defs (Join-Path $common 'can_port_config.h') 2>&1 | Out-Null
        $ErrorActionPreference='Stop'
        if(!$LASTEXITCODE){throw "Invalid/empty port configuration accepted: $bad"}
    }
    & (Join-Path $PSScriptRoot 'test_rh850_can_v4.ps1') -BootProject $BootProject -Matrix
    if($LASTEXITCODE){throw 'Actual v4 multi-port matrix failed.'}
} finally {Pop-Location}
Write-Host 'PASS: production C90 multi-port models; physical CAN/IRQ/Flash acceptance NOT RUN.'
