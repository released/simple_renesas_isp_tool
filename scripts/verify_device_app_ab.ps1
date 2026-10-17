param([Parameter(Mandatory=$true)][string]$BootProject,
      [Parameter(Mandatory=$true)][string]$AppProject,
      [ValidateSet('F13','F24')][string]$Device='F13')
$ErrorActionPreference='Stop'
$boot=(Resolve-Path -LiteralPath $BootProject).Path
$app=(Resolve-Path -LiteralPath $AppProject).Path
$repo=Split-Path -Parent $PSScriptRoot
$root=Join-Path $repo ('tmp\internal\app_ab\'+$Device+'_'+[guid]::NewGuid().ToString('N'))
$compiler='C:\Program Files (x86)\Renesas Electronics\CS+\CC\CubeSuite+.exe'
$bootHex=Join-Path $boot 'DefaultBuild\boot0000_1FFF.hex'
$hash=(Get-FileHash -LiteralPath $bootHex).Hash
function Copy-ProjectSources([string]$Source,[string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | Where-Object {
        $_.Name -notin @('.git','.vs','DefaultBuild','tmp','boot_app.hex','boot_app.bin') -and
        $_.Extension -notin @('.pdf','.jpg','.png','.mtud')
    } | Copy-Item -Destination $Destination -Recurse -Force
}
foreach($variant in @('A','B')) {
    $base=Join-Path $root $variant
    $dest=Join-Path $base ([IO.Path]::GetFileName($app))
    Copy-ProjectSources $app $dest
    $identityPath=Join-Path $dest 'app_image_config.h'
    $identity=[IO.File]::ReadAllText($identityPath)
    $control=if($variant -eq 'A'){1}else{0}
    $identity=[regex]::Replace($identity,'(?m)^(#define\s+APP_IMAGE_VERSION_CONTROL\s+)\([^)]*\)',('${1}('+[string]$control+'U)'))
    [IO.File]::WriteAllText($identityPath,$identity,(New-Object Text.UTF8Encoding($false)))
    # Keep the sibling include/generator contract without invoking a Boot build.
    Copy-ProjectSources $boot (Join-Path $base ([IO.Path]::GetFileName($boot)))
    $vector=if($variant -eq 'A'){'0x0A'}else{'0x0C'}
    $symbol='app_'+$variant.ToLowerInvariant()+'_irq'
    $irqPath=Join-Path $dest 'app_irq_handlers.h'
    $mapping=[IO.File]::ReadAllText($irqPath).TrimEnd()
    $mapping=[regex]::Replace($mapping,'(?m)^#endif\s*$', '')
    $mapping=$mapping.TrimEnd()+" \`n    X($vector, $symbol)`n`n#endif`n"
    [IO.File]::WriteAllText($irqPath,$mapping)
    [IO.File]::WriteAllText((Join-Path $dest 'app_ab_irq.c'),("/* Build-only IRQ fixture; no peripheral is enabled. */`r`n#pragma interrupt $symbol`r`nvoid __near $symbol(void) { }`r`n"))
    $project=@(Get-ChildItem -LiteralPath $dest -Filter '*.mtpj' -File)
    if($project.Count -ne 1){throw 'Expected one App project.'}
    $xml=New-Object Xml.XmlDocument;$xml.PreserveWhitespace=$true;$xml.Load($project[0].FullName)
    $group=$xml.DocumentElement.SelectSingleNode('Class')
    $template=@($group.SelectNodes('Instance[Type="File"]') | Where-Object {
        [IO.Path]::GetExtension([string]$_.RelativePath) -eq '.c'
    })[0]
    if(!$template){throw 'No registered C source template.'}
    $node=$template.CloneNode($true);$node.SetAttribute('Guid',[guid]::NewGuid().ToString())
    $node.SelectSingleNode('Name').InnerText='app_ab_irq.c'
    $node.SelectSingleNode('RelativePath').InnerText='app_ab_irq.c'
    [void]$group.AppendChild($node)
    foreach($item in $xml.SelectNodes('//GenerateFileFolder')){$item.InnerText=$dest}
    $xml.Save($project[0].FullName)
    if($Device -eq 'F24') {
        & (Join-Path (Join-Path $base ([IO.Path]::GetFileName($boot))) 'tools\prepare_projects.ps1') -Kind app -Project $dest
    }
    $log=Join-Path $base 'build.log'
    $process=Start-Process -FilePath $compiler -ArgumentList ('/br "'+$project[0].FullName+'"') -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $log -RedirectStandardError ($log+'.stderr')
    $text=[IO.File]::ReadAllText($log)
    if($text -notmatch 'Build ended\(Error:0, Warning:\d+\)' -or $text -match 'Failed:[1-9]'){throw "App $variant build failed: $log"}
    $table=[IO.File]::ReadAllText((Join-Path $dest 'app_irq_table.inc'))
    if(!$table.Contains('BR !!_'+$symbol)){throw 'Missing independent IRQ table entry.'}
    $image=Join-Path $dest ('DefaultBuild\'+$project[0].BaseName+'_update.bin')
    if(!(Test-Path -LiteralPath $image)){throw 'No packaged App image.'}
    $bytes=[IO.File]::ReadAllBytes($image)
    if([BitConverter]::ToUInt32($bytes,0x174) -ne $control){throw 'Compiled version-control policy mismatch.'}
    if((Get-FileHash -LiteralPath $bootHex).Hash -cne $hash){throw 'Original Boot image changed.'}
    Write-Host "PASS: $Device App $variant clean compiled / linked IRQ / packaged; original Boot SHA256 unchanged."
}
Write-Host "Evidence: $root. Physical IRQ execution NOT RUN."
