param([string]$F13Boot='', [string]$F24Boot='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'workspace_paths.ps1')
$F13Boot=Get-CompanionPath -Kind F13Boot -ExplicitPath $F13Boot
if(!$F24Boot){$F24Boot=Join-Path (Split-Path -Parent $F13Boot) 'RL78_F24_Boot_loader_CAN'}
$canonical=Join-Path (Split-Path -Parent $PSScriptRoot) 'firmware\common\device_metadata'
$jobs=@()
foreach($name in @('image_metadata.c','image_metadata.h')){
    $text=[IO.File]::ReadAllText((Join-Path $canonical $name))
    foreach($root in @($F13Boot,$F24Boot)){
        if(!(Test-Path -LiteralPath $root -PathType Container)){throw "Missing Boot project: $root"}
        $path=Join-Path $root $name
        if($CheckOnly -and (!(Test-Path -LiteralPath $path) -or
            [IO.File]::ReadAllText($path).Replace("`r`n","`n") -cne $text.Replace("`r`n","`n"))){
            throw "Device metadata module differs: $path"
        }
        $jobs+=@([pscustomobject]@{Path=$path;Text=$text})
    }
}
if(!$CheckOnly){
    foreach($job in $jobs){
        [IO.File]::WriteAllText($job.Path,$job.Text,(New-Object Text.UTF8Encoding($false)))
    }
}
Write-Host 'PASS: independent F13/F24 metadata C/header copies match the common source.'
