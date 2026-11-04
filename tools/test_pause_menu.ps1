# TASK-034-3: real-program test of the pause menu (muted, window-only: keys are POSTED to the game window, pictures come from PrintWindow of that window). Needs scratch\local.pmpk.
# Run: powershell -ExecutionPolicy Bypass -File tools/test_pause_menu.ps1   (expects five lines ending in True / exit code 0)
# Real-program test of the pause menu (window-only: keys are POSTED to the game window, the picture is taken with PrintWindow of that window; nothing touches the desktop).
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Runtime.InteropServices; using System.Drawing; using System.IO;
public class W3 {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static byte[] Shot(IntPtr h, string path) {
    RECT r; GetClientRect(h, out r);
    Bitmap bmp = new Bitmap(r.R - r.L, r.B - r.T);
    Graphics g = Graphics.FromImage(bmp); IntPtr dc = g.GetHdc(); PrintWindow(h, dc, 3); g.ReleaseHdc(dc); g.Dispose();
    bmp.Save(path);
    byte[] raw = new byte[bmp.Width * bmp.Height * 4]; int k = 0;
    for (int y = 0; y < bmp.Height; y++) for (int x = 0; x < bmp.Width; x++) { Color c = bmp.GetPixel(x, y); raw[k++] = c.R; raw[k++] = c.G; raw[k++] = c.B; k++; }
    bmp.Dispose(); return raw;
  }
}
"@
function Key($h, $vk) { [void][W3]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]0); Start-Sleep -Milliseconds 150; [void][W3]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]0); Start-Sleep -Milliseconds 150 }
function Same($a, $b) { if ($a.Length -ne $b.Length) { return $false }; for ($i = 0; $i -lt $a.Length; $i++) { if ($a[$i] -ne $b[$i]) { return $false } }; return $true }
$root = Split-Path -Parent $PSScriptRoot
$exe = "F:\Ports\Pac-Man - Apple II\out\bat\pacman_play.exe"
$dir = Join-Path $root "scratch"
$a = "--pack `"$dir\local.pmpk`" --scale 3 --seconds 180 --level 1 --mute"
$p = Start-Process -FilePath $exe -ArgumentList $a -WorkingDirectory $root -PassThru
Start-Sleep -Seconds 2
$p.Refresh(); $h = $p.MainWindowHandle
if ($h -eq 0) { "no window"; Stop-Process -Id $p.Id -Force; exit 1 }
Start-Sleep -Seconds 11   # the jingle (5 s), READY, then the game runs by itself (ghosts move)
$r0 = [W3]::Shot($h, "$dir\m_running.png")
Start-Sleep -Milliseconds 700
$r1 = [W3]::Shot($h, "$dir\m_running2.png")
"game runs by itself (two shots 0.7 s apart differ): " + (-not (Same $r0 $r1))
Key $h 0x1B   # Esc: open the menu
Start-Sleep -Milliseconds 500
$m1 = [W3]::Shot($h, "$dir\m_menu1.png")
Start-Sleep -Seconds 4
$m2 = [W3]::Shot($h, "$dir\m_menu2.png")
"paused: two shots 4 s apart are identical: " + (Same $m1 $m2)
Key $h 0x1B   # Esc: resume
Start-Sleep -Seconds 2
$r2 = [W3]::Shot($h, "$dir\m_resumed.png")
"resumed: the picture is the running game again (differs from the paused one): " + (-not (Same $m2 $r2))
Key $h 0x1B   # menu again: Quit Game -> Cancel -> still running -> Quit Game -> Confirm
Key $h 0x28; Key $h 0x28; Key $h 0x0D   # Down Down Enter: the confirmation page
Start-Sleep -Milliseconds 300
$q1 = [W3]::Shot($h, "$dir\m_confirm.png")
Key $h 0x0D   # Enter on Cancel
Start-Sleep -Milliseconds 500
$p.Refresh()
"cancel keeps the program running: " + (-not $p.HasExited)
Key $h 0x0D   # Enter on Quit Game again: the confirmation page
Start-Sleep -Milliseconds 300
$null = [W3]::Shot($h, "$dir\m_s1.png")
Key $h 0x26   # Up: Confirm
Start-Sleep -Milliseconds 300
$null = [W3]::Shot($h, "$dir\m_s2.png")
Key $h 0x0D   # Enter: quit
$null = $p.WaitForExit(6000)
"quit with Confirm: the program exited: " + $p.HasExited + ", exit code " + $(if ($p.HasExited) { $p.ExitCode } else { "n/a" })
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
