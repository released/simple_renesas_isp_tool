function Set-CanV4Project([xml]$Xml,[string]$Project,[string]$SourceRoot) {
    $files=$Xml.CubeSuiteProject.Class | Where-Object {$_.Guid -eq '68f4a651-b9cd-473b-a595-b00447132ffa'}
    if(!$files){throw 'Unknown CS+ source registration format.'}
    $legacy=@('hal\can\rs_canfd.c','hal\can\rs_canfd.h','hal\can\rs_canfd_regs.h',
        'hal\can\rs_canfd_platform.h','hal\can\rs_canfd_validate.c',
        'hal\can\rs_canfd_sample_app.c','hal\can\rs_canfd_sample_app.h')
    foreach($node in @($files.Instance | Where-Object {$_.Type -eq 'File'})) {
        if([string]$node.RelativePath -in $legacy) {[void]$files.RemoveChild($node)}
    }
    $prefix='hal\can\'
    $names=@('can_service.c','can_service.h','can_board.c','can_board.h','can_config.c','can_config.h','can_user_config.h',
        'rs_canfd.c','rs_canfd.h','rs_canfd_platform.h','rs_canfd_regs.h','rs_canfd_validate.c')
    $parent=[string]$Xml.SelectSingleNode('//FileCategoryGuid').InnerText
    foreach($name in $names) {
        $path=$prefix+$name
        if(!(Test-Path -LiteralPath (Join-Path $Project $path))){throw ('Missing local v4 source: '+$path)}
        if(@($files.Instance | Where-Object {$_.RelativePath -eq $path}).Count){continue}
        $item=$Xml.CreateElement('Instance');$item.SetAttribute('Guid',[guid]::NewGuid().ToString())
        $image=if($path.EndsWith('.c')){'941832c1-fc3b-4e1b-94e8-01ea17128b42'}else{'03cad1e8-2eb3-4cde-a8a3-982423631122'}
        foreach($pair in @{Name=$path;Type='File';RelativePath=$path;TreeImageGuid=$image;ParentItem=$parent}.GetEnumerator()) {
            $child=$Xml.CreateElement($pair.Key);$child.InnerText=$pair.Value;[void]$item.AppendChild($child)
        }
        [void]$files.AppendChild($item)
    }
    $include=$Xml.SelectSingleNode('//COptionI-0')
    $include.InnerText=(@($prefix.TrimEnd('\'))+@($include.InnerText -split '\r?\n' | Where-Object {$_ -and $_ -notmatch '(^|[\\/])hal[\\/]can$'}) | Select-Object -Unique) -join "`n"
}
