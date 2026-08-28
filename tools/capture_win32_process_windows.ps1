param(
    [Parameter(Mandatory = $true)]
    [int]$ProcessId,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [int]$CanvasWidth = 640,
    [int]$CanvasHeight = 480
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class SimTowerProcessWindowCapture
{
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

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
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr targetDc, uint flags);

    public static IntPtr[] WindowsForProcess(uint wantedProcessId)
    {
        var windows = new List<IntPtr>();
        EnumWindows((window, parameter) => {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId == wantedProcessId) windows.Add(window);
            return true;
        }, IntPtr.Zero);
        return windows.ToArray();
    }
}
'@

[SimTowerProcessWindowCapture]::SetProcessDPIAware() | Out-Null
$process = Get-Process -Id $ProcessId
$windows = [SimTowerProcessWindowCapture]::WindowsForProcess([uint32]$ProcessId)
if ($windows.Count -eq 0) {
    throw "Process $ProcessId has no top-level windows"
}

$resolvedParent = Split-Path -Parent $OutputPath
if ($resolvedParent) {
    New-Item -ItemType Directory -Force -Path $resolvedParent | Out-Null
}

$canvas = [System.Drawing.Bitmap]::new(
    $CanvasWidth,
    $CanvasHeight,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
)
$canvasGraphics = [System.Drawing.Graphics]::FromImage($canvas)
$canvasGraphics.Clear([System.Drawing.Color]::FromArgb(192, 192, 192))
$captured = [System.Collections.Generic.List[object]]::new()

try {
    # EnumWindows is top-to-bottom z-order. Paint in reverse so owned dialogs
    # and floating tool windows appear above their process siblings.
    for ($index = $windows.Count - 1; $index -ge 0; --$index) {
        $window = $windows[$index]
        $rect = New-Object SimTowerProcessWindowCapture+RECT
        if (-not [SimTowerProcessWindowCapture]::GetWindowRect($window, [ref]$rect)) {
            continue
        }
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -le 0 -or $height -le 0) {
            continue
        }

        $windowBitmap = [System.Drawing.Bitmap]::new(
            $width,
            $height,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
        )
        $windowGraphics = [System.Drawing.Graphics]::FromImage($windowBitmap)
        $targetDc = $windowGraphics.GetHdc()
        try {
            if (-not [SimTowerProcessWindowCapture]::PrintWindow($window, $targetDc, 2)) {
                continue
            }
        }
        finally {
            $windowGraphics.ReleaseHdc($targetDc)
            $windowGraphics.Dispose()
        }

        try {
            $destination = [System.Drawing.Rectangle]::new($rect.Left, $rect.Top, $width, $height)
            $canvasGraphics.DrawImageUnscaled($windowBitmap, $destination.Location)
            $captured.Add([pscustomobject]@{
                Handle = $window
                Left = $rect.Left
                Top = $rect.Top
                Width = $width
                Height = $height
            })
        }
        finally {
            $windowBitmap.Dispose()
        }
    }
}
finally {
    $canvasGraphics.Dispose()
}

try {
    $canvas.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $canvas.Dispose()
}

[pscustomobject]@{
    ProcessId = $ProcessId
    WindowCount = $captured.Count
    Windows = $captured
    Width = $CanvasWidth
    Height = $CanvasHeight
    OutputPath = (Resolve-Path -LiteralPath $OutputPath).Path
}
