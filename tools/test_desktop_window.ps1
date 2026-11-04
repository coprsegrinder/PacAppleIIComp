# TASK-034-7: real-program test of the Apple II desktop's window: drag, resize, zoom box, kept in the settings (muted; window-only: mouse messages are POSTED to the game window,
# pictures come from PrintWindow of that window; nothing touches the desktop). Uses its own settings file in scratch/. Needs scratch/local.pmpk.
# Run: powershell -ExecutionPolicy Bypass -File tools/test_desktop_window.ps1   (expects every line to end in True; pictures: scratch/desk_1_start.png, desk_2_moved.png, desk_3_small.png, desk_4_fitted.png)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Runtime.InteropServices; using System.Drawing;
public class W7 {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static void Shot(IntPtr h, string path) {
    RECT r; GetClientRect(h, out r);
    Bitmap bmp = new Bitmap(r.R - r.L, r.B - r.T);
    Graphics g = Graphics.FromImage(bmp); IntPtr dc = g.GetHdc(); PrintWindow(h, dc, 3); g.ReleaseHdc(dc); g.Dispose();
    bmp.Save(path); bmp.Dispose();
  }
  public static IntPtr Pack(int x, int y) { return (IntPtr)((y << 16) | (x & 0xFFFF)); }
}
"@
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "scratch"
$ini = Join-Path $dir "test_desktop_settings.ini"
function Mouse($h, $msg, $x, $y, $keys) { [void][W7]::PostMessage($h, $msg, [IntPtr]$keys, [W7]::Pack($x, $y)); Start-Sleep -Milliseconds 40 }
function DragBy($h, $x, $y, $dx, $dy) {
    Mouse $h 0x0201 $x $y 1
    foreach ($i in 1..6) { Mouse $h 0x0200 ($x + [int]($dx * $i / 6)) ($y + [int]($dy * $i / 6)) 1 }
    Mouse $h 0x0202 ($x + $dx) ($y + $dy) 0
    Start-Sleep -Milliseconds 300
}
function Settings() { return (Get-Content $ini -Raw) }
# the program starts windowed, 1920 x 1080 picture area, on the Apple II desktop; the window has a 4x picture at (400, 188) with a 4-pixel unit
"version=1`r`ndisplay_mode=windowed`r`nwindow_borders=1`r`nbackground=desktop`r`nwindow_x=60`r`nwindow_y=60`r`nwindow_w=1920`r`nwindow_h=1080`r`n" | Set-Content $ini -Encoding ASCII
$a = "--pack `"$dir\local.pmpk`" --seconds 90 --level 1 --mute --settings `"$ini`""
$p = Start-Process -FilePath (Join-Path $root "out\bat\pacman_play.exe") -ArgumentList $a -WorkingDirectory $root -PassThru
Start-Sleep -Seconds 13; $p.Refresh(); $h = $p.MainWindowHandle
if ($h -eq 0) { "no window"; Stop-Process -Id $p.Id -Force; exit 1 }
[W7]::Shot($h, "$dir\desk_1_start.png")
$S = 4; $vx = 400; $vy = 188
# 1. drag the title bar by 100, 20
DragBy $h ($vx + 560) ($vy - 6 * $S) 100 20
"drag by the title bar moves the window: " + ((Settings) -match "desktop_x=25\.00" -and (Settings) -match "desktop_y=5\.00")
[W7]::Shot($h, "$dir\desk_2_moved.png")
$vx += 100; $vy += 20
# 2. grow box: resize to scale 2 (the corner of the window frame, then up and to the left)
$gx = $vx + 1120 + 3 * $S - 6; $gy = $vy + 768 + 3 * $S - 6
DragBy $h $gx $gy (-580) (-394)
"grow box resizes to scale 2 (kept in the settings): " + ((Settings) -match "desktop_scale=2")
[W7]::Shot($h, "$dir\desk_3_small.png")
# 3. zoom box: the largest size, centred again (the settings have nothing left to keep)
$zx = ($vx + 560 + 280 * 2 / 2) ; $zy = 0
$w1 = $vx + 560 + 3 * 2   # right edge of the small window (picture 560 wide at scale 2, frame 3 units of 2 pixels)
$zx = $w1 - 5 * 2 - 2 * 2 + 2 * 2   # the middle of the zoom box: 4.5 units from the right edge
$zy = $vy - 11 * 2 + 4 * 2
Mouse $h 0x0201 ($w1 - 9) $zy 1
Mouse $h 0x0202 ($w1 - 9) $zy 0
Start-Sleep -Milliseconds 400
"zoom box fits the window again (nothing of the desktop window left in the settings): " + (-not ((Settings) -match "desktop_scale") -and -not ((Settings) -match "desktop_x"))
[W7]::Shot($h, "$dir\desk_4_fitted.png")
"program still running: " + (-not $p.HasExited)
Stop-Process -Id $p.Id -Force
Remove-Item $ini -ErrorAction SilentlyContinue
