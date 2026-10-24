# Resolve explicitly owned chapters; never search and update an arbitrary duplicate.
function Get-WebGuideSectionPath {
    param([string]$WebRoot,[string]$Folder,[string]$Section)
    $layout=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'web_guide_layout.json')) | ConvertFrom-Json
    $site=$layout.PSObject.Properties[$Folder]
    if(!$site){throw "Unknown guide site: $Folder"}
    $entry=$site.Value.sections.PSObject.Properties[$Section]
    if(!$entry){throw "Missing chapter placement: $Folder / $Section"}
    return Join-Path $WebRoot ($Folder+'\'+$entry.Value+'.md')
}

function Get-WebGuideReaderPaths {
    param([string]$WebRoot,[string]$Folder)
    $layout=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'web_guide_layout.json')) | ConvertFrom-Json
    foreach($page in $layout.PSObject.Properties[$Folder].Value.pages.PSObject.Properties.Name){
        Join-Path $WebRoot ($Folder+'\'+$page+'.md')
    }
}
