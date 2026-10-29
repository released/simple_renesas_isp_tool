param(
    [Parameter(Mandatory=$true)][string[]]$DeviceBootProjects,
    [string[]]$InputBin=@(),
    [string]$Compiler='C:\Program Files (x86)\Renesas Electronics\CS+\CC\CC-RL\V1.15.01\bin\ccrl.exe'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'device_hal_paths.ps1')
$projects=@($DeviceBootProjects | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
$bins=@($InputBin | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
$sources=@($projects | ForEach-Object { Get-DeviceProjectSource $_ 'sw_crc32.c' })
$hashes=@($sources | ForEach-Object { (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash } | Sort-Object -Unique)
if($hashes.Count -ne 1){throw 'Device CRC source copies differ.'}
. (Join-Path $PSScriptRoot 'test_owned_workspace.ps1')
$testScope=New-OwnedTestWorkspace 'device_crc_boundary'
$out=$testScope.Path
try {
$vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC developer tools are required for native checks.'}
$lines=& cmd /d /c ('call "'+(Join-Path $vs 'Common7\Tools\VsDevCmd.bat')+'" -no_logo -arch=amd64 >nul && set')
if($LASTEXITCODE){throw 'MSVC environment failed.'}
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
function Assert-AddressCarry([string]$Asm){
    $increment=[regex]::Match($Asm,'(?s);\*\*\*[^\r\n]*data = \(const unsigned char __far \*\).*?(?<body>addw\s+ax,\s*#0x0001.*?);\*\*\*[^\r\n]*bit = 8U;')
    if(!$increment.Success){throw 'Missing target integer-address increment evidence.'}
    $instructions=$increment.Groups['body'].Value -replace '(?m)^\.[^\r\n]*',''
    if($instructions -notmatch '(?s)addw\s+ax,\s*#0x0001.*?sknc\s+incw\s+ax'){
        throw 'Target address increment lacks the reviewed low-word carry / high-word increment. Review compiler assembly.'
    }
    if($Asm -notmatch 'mov\s+a,\s*b\s+mov\s+es,\s*a' -or
       $instructions -notmatch 'mov\s+a,\s*x\s+mov\s+\[sp\+0x[0-9A-F]+\],\s*a'){
        throw 'Target read does not reload the address segment as reviewed.'
    }
}
foreach($project in $projects){
    $name=Split-Path -Leaf $project
    $target=Join-Path $out $name
    New-Item -ItemType Directory -Path $target | Out-Null
    Push-Location -LiteralPath $target
    try{
        & cl /nologo /std:c++17 /W4 /WX /EHsc ('/I'+$project) ('/I'+(Join-Path $project 'hal\image')) (Join-Path $repo 'tests\device_crc_boundary_tests.cpp') /Fe:crc_boundary.exe
        if($LASTEXITCODE){throw "Native CRC build failed: $name"}
        & (Join-Path $target 'crc_boundary.exe') @bins
        if($LASTEXITCODE){throw "Native CRC checks failed: $name"}
        & $Compiler -S -cpu=S3 -Olite -lang=c -strict_std -pass_source -o crc_target.asm (Get-DeviceProjectSource $project 'sw_crc32.c')
        if($LASTEXITCODE){throw "CC-RL CRC compilation failed: $name"}
        $asm=Get-Content -LiteralPath 'crc_target.asm' -Raw
        Assert-AddressCarry $asm
        $source=Get-Content -LiteralPath (Get-DeviceProjectSource $project 'sw_crc32.c') -Raw
        $safe='data = (const unsigned char __far *)((f13_crc_address_t)data + 1UL);'
        if(!$source.Contains($safe)){throw 'Missing mutation anchor.'}
        # Generated local negative fixture; never modify the selected project.
        [IO.File]::WriteAllText((Join-Path $target 'unsafe.c'),$source.Replace($safe,'data++;'))
        & $Compiler -S -cpu=S3 -Olite -lang=c -strict_std -pass_source ('-I'+$project) ('-I'+(Join-Path $project 'hal\image')) -o unsafe.asm unsafe.c
        if($LASTEXITCODE){throw 'Unsafe regression fixture did not compile.'}
        $rejected=$false
        try{Assert-AddressCarry (Get-Content -LiteralPath 'unsafe.asm' -Raw)}catch{
            if($_.Exception.Message -notmatch 'integer-address increment|lacks the reviewed|does not reload'){throw}
            $rejected=$true
        }
        if(!$rejected){throw 'Target guard accepted far-pointer increment regression.'}
        Write-Host "PASS: $name native CRC and CC-RL 32-bit address carry / segment reload."
        Write-Host 'PASS: compiled far-pointer-increment mutation rejected.'
    }finally{Pop-Location}
}
Write-Host "Evidence: $out"
Write-Host 'Physical Flash/update/timeout acceptance NOT RUN.'

} finally { Remove-OwnedTestWorkspace $testScope }
