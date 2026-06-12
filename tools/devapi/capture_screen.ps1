param(
    [string]$Output = "build/captures/screenshot.png",
    [int]$X = 0,
    [int]$Y = 0,
    [int]$Width = 0,
    [int]$Height = 0,
    [int]$ProcessId = 0,
    [string]$ProcessName = "game_seed"
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
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class Win32Capture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
}
"@

$targetHandle = [IntPtr]::Zero

if ($Width -le 0 -or $Height -le 0) {
    if ($ProcessId -gt 0) {
        $target = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    } else {
        $target = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowHandle -ne 0 } |
            Select-Object -First 1
    }
    if ($null -ne $target) {
        $deadline = (Get-Date).AddSeconds(3)
        while ($target.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 100
            $target.Refresh()
        }
        $rect = New-Object Win32Capture+RECT
        if ([Win32Capture]::GetWindowRect($target.MainWindowHandle, [ref]$rect)) {
            $targetHandle = $target.MainWindowHandle
            [Win32Capture]::ShowWindow($targetHandle, 9) | Out-Null
            [Win32Capture]::SetWindowPos($targetHandle, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
            [Win32Capture]::SetForegroundWindow($target.MainWindowHandle) | Out-Null
            Start-Sleep -Milliseconds 250
            $X = $rect.Left
            $Y = $rect.Top
            $Width = $rect.Right - $rect.Left
            $Height = $rect.Bottom - $rect.Top
        }
    }
    if ($Width -le 0 -or $Height -le 0) {
        if ($ProcessId -gt 0 -or ![string]::IsNullOrWhiteSpace($ProcessName)) {
            throw "Could not resolve a visible game window for capture."
        } else {
            $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
            $X = $bounds.X
            $Y = $bounds.Y
            $Width = $bounds.Width
            $Height = $bounds.Height
        }
    }
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
    if ($targetHandle -ne [IntPtr]::Zero) {
        [Win32Capture]::SetWindowPos($targetHandle, [IntPtr]::new(-2), 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
    }
}
