param()
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName PresentationCore,WindowsBase
[xml]$svg=[IO.File]::ReadAllText((Join-Path $root 'res\app_icon.svg'))
$rect=$svg.svg.rect
$path=$svg.svg.path
$brushConverter=New-Object Windows.Media.BrushConverter
$background=$brushConverter.ConvertFromInvariantString([string]$rect.fill)
$foreground=$brushConverter.ConvertFromInvariantString([string]$path.fill)
$geometry=[Windows.Media.Geometry]::Parse([string]$path.d)
$frames=New-Object 'Collections.Generic.List[byte[]]'
$sizes=@(16,24,32,48,64,128,256)
foreach($size in $sizes){
    $visual=New-Object Windows.Media.DrawingVisual
    $context=$visual.RenderOpen()
    try {
        $context.PushTransform((New-Object Windows.Media.ScaleTransform ($size/256.0),($size/256.0)))
        $context.DrawRoundedRectangle($background,$null,(New-Object Windows.Rect 0,0,256,256),[double]$rect.rx,[double]$rect.rx)
        $context.DrawGeometry($foreground,$null,$geometry)
        $context.Pop()
    } finally {$context.Close()}
    $bitmap=New-Object Windows.Media.Imaging.RenderTargetBitmap $size,$size,96,96,([Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)
    $encoder=New-Object Windows.Media.Imaging.PngBitmapEncoder
    $encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $stream=New-Object IO.MemoryStream
    try {$encoder.Save($stream);$frames.Add($stream.ToArray())} finally {$stream.Dispose()}
}
# Windows Vista and later accept PNG-compressed entries at every ICO size.
$output=New-Object IO.MemoryStream
$writer=New-Object IO.BinaryWriter $output
try {
    $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
    $offset=6+16*$sizes.Count
    for($i=0;$i -lt $sizes.Count;$i++){
        $dimension=if($sizes[$i] -eq 256){0}else{$sizes[$i]}
        $writer.Write([byte]$dimension);$writer.Write([byte]$dimension)
        $writer.Write([byte]0);$writer.Write([byte]0)
        $writer.Write([uint16]1);$writer.Write([uint16]32)
        $writer.Write([uint32]$frames[$i].Length);$writer.Write([uint32]$offset)
        $offset+=$frames[$i].Length
    }
    foreach($frame in $frames){$writer.Write($frame)}
    $writer.Flush()
    [IO.File]::WriteAllBytes((Join-Path $root 'res\app_icon.ico'),$output.ToArray())
} finally {$writer.Dispose();$output.Dispose()}
Write-Host 'Generated R icon:16/24/32/48/64/128/256 pixels.'
