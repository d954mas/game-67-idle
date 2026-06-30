param(
    [string]$Output = "build/captures/gameplay.mp4",
    [int]$Seconds = 8,
    [int]$Framerate = 30,
    [int]$X = 0,
    [int]$Y = 0,
    [int]$Width = 0,
    [int]$Height = 0
)

$ErrorActionPreference = "Stop"
$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
if ($null -eq $ffmpeg) {
    throw "ffmpeg is not available in PATH. Install ffmpeg or use ai_studio/runtime_automation/capture_screen.ps1 for still screenshots."
}

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")
$outPath = if ([System.IO.Path]::IsPathRooted($Output)) { $Output } else { Join-Path $root $Output }
$outDir = Split-Path -Parent $outPath
if (!(Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

if ($Width -le 0 -or $Height -le 0) {
    Add-Type -AssemblyName System.Windows.Forms
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $X = $bounds.X
    $Y = $bounds.Y
    $Width = $bounds.Width
    $Height = $bounds.Height
}

$offset = "$X,$Y"
$size = "${Width}x${Height}"
& $ffmpeg.Source -y -f gdigrab -framerate $Framerate -offset_x $X -offset_y $Y -video_size $size -t $Seconds -i desktop -pix_fmt yuv420p $outPath
Write-Host "Saved recording: $outPath"
