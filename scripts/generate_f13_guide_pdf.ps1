[CmdletBinding()]
param(
    [string]$Source = "docs\F13_BOOT_UPDATE_FLOW_zh-TW.md",
    [string]$Output = "docs\F13_CAN_Bootloader_Update_Guide_zh-TW.pdf",
    [int]$ExpectedDiagramCount = 10,
    [string]$Title = "RL78/F13 CAN Boot Loader and Firmware Update Guide (zh-TW)",
    [string]$Footer = "RL78/F13 CAN Boot Loader and Firmware Update"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $projectRoot $Source
$outputPath = Join-Path $projectRoot $Output
$guideName = [System.IO.Path]::GetFileNameWithoutExtension($sourcePath)
$workDirectory = Join-Path $projectRoot ("tmp\pdfs\" + $guideName)
$renderedMarkdown = Join-Path $workDirectory "F13_BOOT_UPDATE_FLOW_rendered.md"
$diagramDirectory = Join-Path $workDirectory "diagrams"

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Guide source not found: $sourcePath"
}

New-Item -ItemType Directory -Path $workDirectory -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null

$npx = (Get-Command npx -ErrorAction Stop).Source
& $npx -y "@mermaid-js/mermaid-cli" `
    -i $sourcePath `
    -o $renderedMarkdown `
    -a $diagramDirectory `
    -e png `
    -b white `
    --size 1800 `
    -j 4
if ($LASTEXITCODE -ne 0) {
    throw "Mermaid rendering failed."
}

& python (Join-Path $PSScriptRoot "generate_f13_guide_pdf.py") `
    --source $renderedMarkdown `
    --output $outputPath --title $Title --footer $Footer
if ($LASTEXITCODE -ne 0) {
    throw "PDF generation failed."
}

$diagramCount = @(Get-ChildItem -LiteralPath $diagramDirectory -Filter "*.png").Count
if ($diagramCount -ne $ExpectedDiagramCount) {
    throw "Expected $ExpectedDiagramCount Mermaid diagrams, found $diagramCount."
}

Write-Host "Generated PDF: $outputPath"
Write-Host "Rendered Mermaid diagrams: $diagramCount"
