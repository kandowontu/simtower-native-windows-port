param(
    [Parameter(Mandatory = $true)]
    [int]$ProcessId,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class SimTowerWindowCapture
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool SetProcessDPIAware();

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr window, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr targetDc, uint flags);
}
'@

[SimTowerWindowCapture]::SetProcessDPIAware() | Out-Null
$process = Get-Process -Id $ProcessId
if ($process.MainWindowHandle -eq [IntPtr]::Zero) {
    throw "Process $ProcessId has no main window"
}

$rect = New-Object SimTowerWindowCapture+RECT
if (-not [SimTowerWindowCapture]::GetClientRect($process.MainWindowHandle, [ref]$rect)) {
    throw 'GetClientRect failed'
}

$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    throw "Invalid client dimensions ${width}x${height}"
}

$resolvedParent = Split-Path -Parent $OutputPath
if ($resolvedParent) {
    New-Item -ItemType Directory -Force -Path $resolvedParent | Out-Null
}

$bitmap = [System.Drawing.Bitmap]::new(
    $width,
    $height,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$targetDc = $graphics.GetHdc()
try {
    # PW_CLIENTONLY | PW_RENDERFULLCONTENT captures the guest framebuffer even
    # when another desktop window overlaps the emulator.
    if (-not [SimTowerWindowCapture]::PrintWindow($process.MainWindowHandle, $targetDc, 3)) {
        throw 'PrintWindow failed'
    }
}
finally {
    $graphics.ReleaseHdc($targetDc)
    $graphics.Dispose()
}

try {
    $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $bitmap.Dispose()
}

[pscustomobject]@{
    ProcessId = $ProcessId
    Width = $width
    Height = $height
    OutputPath = (Resolve-Path -LiteralPath $OutputPath).Path
}
