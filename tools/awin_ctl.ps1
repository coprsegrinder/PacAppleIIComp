<# Dev-only helper to drive a running AppleWin without needing keyboard focus.
   Usage: powershell -File tools/awin_ctl.ps1 -Text "BSAVE ..." -Shot out.png [-Key F7]
   Sends WM_CHAR messages straight to the AppleWin window and captures ONLY that window (never the desktop). #>
param([string]$Text = "", [string]$Key = "", [string]$Shot = "")
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public class AwinCtl {
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RC r);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
 public struct RC { public int L, T, R, B; } }
'@
$p = Get-Process AppleWin-x64 -ErrorAction Stop | Select-Object -First 1
$h = [IntPtr]$p.MainWindowHandle
if ([AwinCtl]::IsIconic($h)) { [void][AwinCtl]::ShowWindow($h, 4) }   # restore without stealing focus
$vk = @{ F7 = 0x76; F2 = 0x71; Esc = 0x1B }
if ($Key) { [void][AwinCtl]::PostMessage($h, 0x100, [IntPtr]$vk[$Key], [IntPtr]1); Start-Sleep -Milliseconds 150
            [void][AwinCtl]::PostMessage($h, 0x101, [IntPtr]$vk[$Key], [IntPtr]0xC0000001) }
foreach ($c in $Text.ToCharArray()) { [void][AwinCtl]::PostMessage($h, 0x102, [IntPtr][int]$c, [IntPtr]1); Start-Sleep -Milliseconds 30 }
if ($Text) { [void][AwinCtl]::PostMessage($h, 0x100, [IntPtr]0x0D, [IntPtr]1); Start-Sleep -Milliseconds 100
             [void][AwinCtl]::PostMessage($h, 0x102, [IntPtr]13, [IntPtr]1) }
Start-Sleep -Seconds 1
if ($Shot) {
  $r = New-Object AwinCtl+RC; [void][AwinCtl]::GetClientRect($h, [ref]$r)
  $bmp = New-Object Drawing.Bitmap $r.R, $r.B; $g = [Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
  [void][AwinCtl]::PrintWindow($h, $dc, 3); $g.ReleaseHdc($dc); $bmp.Save($Shot) }
