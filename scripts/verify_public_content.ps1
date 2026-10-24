param([string]$WebRoot='', [string[]]$TextPath=@())
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(!$TextPath.Count) {
    . (Join-Path $PSScriptRoot 'workspace_paths.ps1')
    $WebRoot=Get-CompanionPath -Kind Web -ExplicitPath $WebRoot
    $TextPath=@('README.md','docs\CAN_UPDATE_WALKTHROUGH_zh-TW.md',
        'docs\DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md','docs\FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md',
        'docs\F13_BOOT_UPDATE_FLOW_zh-TW.md','docs\F24_CAN_DEVICE_DESIGN_zh-TW.md',
        'docs\RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md','docs\MULTIBUS_TOOL_GUIDE_zh-TW.md',
        'docs\SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md','docs\UPDATE_VALIDATION_zh-TW.md',
        'docs\COMMON_IMAGE_METADATA.md') | ForEach-Object { Join-Path $repo $_ }
    foreach($folder in @('slide_boot_loader_for_RL78','slide_isp_tool_for_RL78')) {
        . (Join-Path $PSScriptRoot 'web_guide_layout.ps1')
        $TextPath+=@(Get-WebGuideReaderPaths -WebRoot $WebRoot -Folder $folder)
        $TextPath+=Join-Path $WebRoot ($folder+'\README.md')
    }
    foreach($name in @('slide_boot_loader_for_RL78\uart_i2c\index.md',
            'released.github.io\README.md','released.github.io\README.html')) {
        $TextPath+=Join-Path $WebRoot $name
    }
}
# Deliberately narrow checks, not a replacement for contextual semantic review.
# Unicode escapes keep the source safe in Windows PowerShell 5.1 without a BOM.
$rules=@(
    @{Id='private-path'; Pattern='(?<![A-Za-z])[A-Za-z]:[\\/]|_Avery|SourceCode'},
    @{Id='invalid-glyph'; Pattern='[\uE000-\uF8FF\uFFFD]'},
    @{Id='publication-label'; Pattern='\u5c1a\u672a\u767c\u5e03|\u672c\u6a5f\u7de8\u8f2f\u7a3f|\u5de5\u7a0b\u7b2c\u4e00\u7248'},
    @{Id='publication-plan'; Pattern='GitHub[^\r\n]*(?:\u5c07\u65bc|\u9a57\u8b49\u5f8c\u63d0\u4f9b)'},
    @{Id='development-dialogue'; Pattern='\u4f9d\u4f7f\u7528\u8005\u8981\u6c42|\u672c\u8f2a|\u672c\u6b21\u4fee\u6539|\u958b\u767c\u8005\u8207\s*Codex'},
    @{Id='reader-judgment'; Pattern='\u4f60\u5fc5\u9808\u77e5\u9053|\u4f60\u61c9\u8a72\u77e5\u9053|\u5148\u641e\u6e05\u695a'},
    @{Id='corrective-heading'; Pattern='^#{1,6}\s+.*(?:\u5148\u5206\u6e05|\u5148\u8fa8\u8b58|\u4e0d\u8981\u6df7\u6dc6|\u61c9\u770b\u5230\u4ec0\u9ebc)'}
)
$findings=New-Object 'Collections.Generic.List[string]'
foreach($path in $TextPath) {
    $lines=[IO.File]::ReadAllLines($path)
    $fenceChar=''; $fenceLength=0; $number=0
    foreach($line in $lines) {
        $number++
        # Paths in command samples are also public; check before fence filtering.
        foreach($rule in $rules | Select-Object -First 2) {
            if($line -match $rule.Pattern) { $findings.Add("${path}:${number} [$($rule.Id)]") }
        }
        if($line -match '^\s*(`{3,}|~{3,})(.*)$') {
            $marker=$Matches[1]; $tail=$Matches[2]
            if(!$fenceLength) { $fenceChar=$marker.Substring(0,1);$fenceLength=$marker.Length }
            elseif($marker.Substring(0,1) -eq $fenceChar -and $marker.Length -ge $fenceLength -and !$tail.Trim()) {
                $fenceLength=0;$fenceChar=''
            }
            continue
        }
        if($fenceLength) { continue }
        foreach($rule in $rules | Select-Object -Skip 2) {
            if($line -match $rule.Pattern) { $findings.Add("${path}:${number} [$($rule.Id)]") }
        }
    }
    if($fenceLength) { $findings.Add("${path} [unclosed-fence]") }
}
if($findings.Count) {
    $findings | Write-Output
    throw "Public-content checks found $($findings.Count) issue(s); review context before editing."
}
Write-Host "PASS: mechanical public-content checks ($($TextPath.Count) files); semantic review remains required."
