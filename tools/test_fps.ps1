# TASK-034-11: real-program test of the FPS setting (muted, window-only, own settings and log files in scratch/; nothing touches the desktop). For every FPS setting the real game runs
# for a while and the program's own log says (a) how many pictures the window showed and how steady they were, and (b) how many game ticks ran in how much real time: the pictures follow the
# setting, the game's speed does not. Needs scratch/local.pmpk. Run: powershell -ExecutionPolicy Bypass -File tools/test_fps.ps1   (expects every line to end in True)
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "scratch"
$exe = Join-Path $root "out\bat\pacman_play.exe"
$pack = Join-Path $dir "local.pmpk"
$seconds = 12
$results = @{}
$expected = @{ "native" = 60.05; "30" = 30; "60" = 60; "120" = 120; "144" = 144 }
foreach ($mode in @("native", "30", "60", "120", "144", "unlocked")) {
    $ini = Join-Path $dir "test_fps_settings.ini"
    $log = Join-Path $dir "test_fps_log.txt"
    Remove-Item $log -ErrorAction SilentlyContinue
    "version=1`r`ndisplay_mode=windowed`r`nwindow_borders=1`r`nbackground=black`r`nwindow_x=60`r`nwindow_y=60`r`nwindow_w=840`r`nwindow_h=576`r`nfps=$mode`r`n" | Set-Content $ini -Encoding ASCII
    $a = "--pack `"$pack`" --level 1 --seconds $seconds --mute --settings `"$ini`" --log `"$log`""
    $p = Start-Process -FilePath $exe -ArgumentList $a -WorkingDirectory $root -PassThru
    [void]$p.WaitForExit(($seconds + 15) * 1000)
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    $text = if (Test-Path $log) { Get-Content $log -Raw } else { "" }
    $f = [regex]::Match($text, "frames: fps=(\S+) shown=(\d+) mean_ms=([\d.]+) max_ms=([\d.]+) deviation_ms=([\d.]+)")
    $g = [regex]::Match($text, "game: ticks=(\d+) seconds=([\d.]+)")
    if (-not $f.Success -or -not $g.Success) { "$mode : no frame / game line in the log: False"; continue }
    $shown = [double]$f.Groups[2].Value; $mean = [double]$f.Groups[3].Value; $dev = [double]$f.Groups[5].Value
    $ticks = [double]$g.Groups[1].Value; $secs = [double]$g.Groups[2].Value
    $rate = 1000.0 / $mean
    $tps = if ($secs -gt 0) { $ticks / $secs } else { 0 }
    $results[$mode] = @{ rate = $rate; tps = $tps }
    "{0,-9} pictures: {1} shown, mean interval {2:N2} ms = {3:N1} a second, deviation {4:N2} ms; game: {5} ticks in {6:N2} s = {7:N1} ticks a second" -f $mode, $shown, $mean, $rate, $dev, $ticks, $secs, $tps
}
foreach ($mode in $expected.Keys) {
    if ($results.ContainsKey($mode)) {
        $r = $results[$mode].rate
        "$mode : the picture rate is the setting (within 5 %): " + (([math]::Abs($r - $expected[$mode]) -lt 0.05 * $expected[$mode]))
    }
}
if ($results.ContainsKey("unlocked")) { "unlocked : more pictures than the 144 cap allows: " + ($results["unlocked"].rate -gt 150) }
# the game's speed (ticks a second from the first tick on): the same whatever the picture rate (the game runs from its own clock)
$tpsAll = $results.Values | ForEach-Object { $_.tps }
if ($tpsAll.Count -gt 1) {
    $min = ($tpsAll | Measure-Object -Minimum).Minimum; $max = ($tpsAll | Measure-Object -Maximum).Maximum
    "game speed the same in every setting (ticks a second {0:N1} .. {1:N1}, within 3 %): {2}" -f $min, $max, (($max - $min) / $max -lt 0.03)
}
