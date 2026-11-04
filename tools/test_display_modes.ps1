# TASK-034-4: real-program test of the display modes and the settings file (muted; window-only: keys are POSTED to the game window; nothing touches the desktop).
# Uses its own settings file in scratch/ (never the real one). Needs scratch/local.pmpk.
# Run: powershell -ExecutionPolicy Bypass -File tools/test_display_modes.ps1   (expects every line to end in True)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W5 {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
}
"@
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "scratch"
$exe = Join-Path $root "out\bat\pacman_play.exe"
$settings = Join-Path $dir "test_display_settings.ini"
$log = Join-Path $dir "test_display_log.txt"
function Key($h, $vk) { [void][W5]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]0); Start-Sleep -Milliseconds 150; [void][W5]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]0); Start-Sleep -Milliseconds 250 }
function Launch($extra) {
    $a = "--pack `"$dir\local.pmpk`" --seconds 120 --level 1 --mute --settings `"$settings`" --log `"$log`" " + $extra
    $p = Start-Process -FilePath $exe -ArgumentList $a -WorkingDirectory $root -PassThru
    Start-Sleep -Seconds 3; $p.Refresh()
    return $p
}
function LastLog() { if (Test-Path $log) { return (Get-Content $log | Select-Object -Last 1) } else { return "" } }
$mw = [W5]::GetSystemMetrics(0); $mh = [W5]::GetSystemMetrics(1)
Remove-Item $settings, $log -ErrorAction SilentlyContinue

# 1. first run without any settings file: borderless full screen at the desktop's own resolution
$p = Launch ""
$h = $p.MainWindowHandle
"first run (no settings file) starts borderless at the desktop resolution ${mw}x${mh}: " + ((LastLog) -match "mode=borderless" -and (LastLog) -match "client=${mw}x${mh}")

# 2. the menu changes the mode: Esc, Down, Enter (graphics page), Right (borderless -> windowed), Left (-> borderless), Right again
Key $h 0x1B; Key $h 0x28; Key $h 0x0D
Key $h 0x27
"menu Right: windowed: " + ((LastLog) -match "mode=windowed" -and -not ((LastLog) -match "client=${mw}x${mh}"))
Key $h 0x25
"menu Left: borderless again: " + ((LastLog) -match "mode=borderless" -and (LastLog) -match "client=${mw}x${mh}")
Key $h 0x27   # windowed
$listed = @("2560x1080","3440x1440","1920x1080","2560x1440","3840x2160","1280x720","1600x900","1366x768","1680x1050","1920x1200","1024x768","1280x960")
Key $h 0x28; Key $h 0x27   # Down (Resolution), Right: the next size that fits this monitor
$line = LastLog
$size = [regex]::Match($line, "client=(\d+x\d+)").Groups[1].Value
"Resolution entry (Right) sets the picture area to a listed size ($size): " + ($line -match "^resolution:" -and $listed -contains $size)
Key $h 0x27
$line2 = LastLog
$size2 = [regex]::Match($line2, "client=(\d+x\d+)").Groups[1].Value
"Right again gives another listed size ($size2): " + ($listed -contains $size2 -and $size2 -ne $size)
Key $h 0x25
"Left goes back to $size : " + ([regex]::Match((LastLog), "client=(\d+x\d+)").Groups[1].Value -eq $size)
Key $h 0x1B; Key $h 0x1B   # back to the game
$null = Start-Sleep -Milliseconds 500
$text = Get-Content $settings -Raw
"settings file written on the change (display_mode=windowed): " + ($text -match "display_mode=windowed")

# 3. quit through the menu (Quit Game, Confirm): exit code 0, window place and size are in the file
Key $h 0x1B; Key $h 0x26; Key $h 0x0D; Key $h 0x26; Key $h 0x0D
$null = $p.WaitForExit(6000)
"quit through the menu: exited " + $p.HasExited + ", code " + $(if ($p.HasExited) { $p.ExitCode } else { "n/a" })
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
$text = Get-Content $settings -Raw
"settings kept the window (window_w, window_h) and the mode: " + ($text -match "window_w=\d+" -and $text -match "window_h=\d+" -and $text -match "display_mode=windowed")
$w = [int]([regex]::Match($text, "window_w=(\d+)").Groups[1].Value); $hh = [int]([regex]::Match($text, "window_h=(\d+)").Groups[1].Value)

# 4. the next start: the same mode and the same window size, without any command-line choice
Remove-Item $log -ErrorAction SilentlyContinue
$p = Launch ""
"restart restores windowed ${w}x${hh}: " + ((LastLog) -match "mode=windowed" -and (LastLog) -match "client=${w}x${hh}")
Stop-Process -Id $p.Id -Force

# 5. a damaged settings file does not stop the program (defaults: borderless)
[IO.File]::WriteAllBytes($settings, [byte[]](0xFF, 0xFE, 0x00, 0x41, 0x3D, 0x00, 0x0A, 0x3D, 0x3D))
Remove-Item $log -ErrorAction SilentlyContinue
$p = Launch ""
"damaged settings file: the program starts with the defaults: " + ((LastLog) -match "mode=borderless" -and -not $p.HasExited)
Stop-Process -Id $p.Id -Force
Remove-Item $settings, $log -ErrorAction SilentlyContinue
