param(
    [Parameter(Mandatory = $true)]
    [long]$WindowHandle,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class SimTowerSingleWindowCapture
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
    public static extern bool GetWindowRect(IntPtr window, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr targetDc, uint flags);
}
'@

[SimTowerSingleWindowCapture]::SetProcessDPIAware() | Out-Null
$window = [IntPtr]$WindowHandle
$rect = New-Object SimTowerSingleWindowCapture+RECT
if (-not [SimTowerSingleWindowCapture]::GetWindowRect($window, [ref]$rect)) {
    throw 'GetWindowRect failed'
}
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    throw "Invalid window dimensions ${width}x${height}"
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
    if (-not [SimTowerSingleWindowCapture]::PrintWindow($window, $targetDc, 2)) {
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
    WindowHandle = ('0x{0:X}' -f $WindowHandle)
    Width = $width
    Height = $height
    OutputPath = (Resolve-Path -LiteralPath $OutputPath).Path
}
