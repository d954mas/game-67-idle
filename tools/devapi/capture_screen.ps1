param(
    [string]$Output = "build/captures/screenshot.png",
    [int]$X = 0,
    [int]$Y = 0,
    [int]$Width = 0,
    [int]$Height = 0
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")
$outPath = if ([System.IO.Path]::IsPathRooted($Output)) { $Output } else { Join-Path $root $Output }
$outDir = Split-Path -Parent $outPath
if (!(Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

if ($Width -le 0 -or $Height -le 0) {
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $X = $bounds.X
    $Y = $bounds.Y
    $Width = $bounds.Width
    $Height = $bounds.Height
}

$bitmap = New-Object System.Drawing.Bitmap $Width, $Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen($X, $Y, 0, 0, $bitmap.Size)
    $bitmap.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Saved screenshot: $outPath"
}
finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}
