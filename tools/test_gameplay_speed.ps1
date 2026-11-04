# TASK-034-11 (Gameplay Speed): real-program test (muted, own settings and log files in scratch/, no keys, nothing touches the desktop). The same level is played idle for the same time in
# Original and in Steady; the program's own log says how many ticks ran, how many of them changed the picture, and what the ticks cost. Needs scratch/local.pmpk.
# Run: powershell -ExecutionPolicy Bypass -File tools/test_gameplay_speed.ps1   (expects every line to end in True)
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "scratch"
$exe = Join-Path $root "out\bat\pacman_play.exe"
$pack = Join-Path $dir "local.pmpk"
$res = @{}
foreach ($mode in @("original", "steady")) {
    $ini = Join-Path $dir "test_speed_settings.ini"; $log = Join-Path $dir "test_speed_log.txt"
    Remove-Item $log -ErrorAction SilentlyContinue
    "version=1`r`ndisplay_mode=windowed`r`nwindow_x=60`r`nwindow_y=60`r`nwindow_w=840`r`nwindow_h=576`r`ngameplay_speed=$mode`r`n" | Set-Content $ini -Encoding ASCII
    $p = Start-Process -FilePath $exe -ArgumentList "--pack `"$pack`" --level 1 --seconds 22 --mute --settings `"$ini`" --log `"$log`"" -WorkingDirectory $root -PassThru
    [void]$p.WaitForExit(60000)
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    $text = if (Test-Path $log) { Get-Content $log -Raw } else { "" }
    $g = [regex]::Match($text, "game: ticks=(\d+) seconds=([\d.]+) picture_changes=(\d+) tick_cycles_min=(\d+) mean=(\d+) max=(\d+) freeze_cycles_total=(\d+) slow_ticks=(\d+)")
    if (-not $g.Success) { "$mode : no game line in the log: False"; continue }
    $res[$mode] = @{ ticks = [double]$g.Groups[1].Value; secs = [double]$g.Groups[2].Value; changes = [double]$g.Groups[3].Value; min = [double]$g.Groups[4].Value; mean = [double]$g.Groups[5].Value; max = [double]$g.Groups[6].Value; slow = [double]$g.Groups[8].Value }
    $r = $res[$mode]
    "{0,-9} {1} ticks in {2:N2} s ({3:N1} a second), {4} changed the picture; tick cost min {5} mean {6} max {7} cycles; {8} ticks over twice the typical" -f $mode, $r.ticks, $r.secs, ($r.ticks / $r.secs), $r.changes, $r.min, $r.mean, $r.max, $r.slow
}
if ($res.ContainsKey("original") -and $res.ContainsKey("steady")) {
    $o = $res["original"]; $s = $res["steady"]
    "the same logic: the same ticks give the same number of picture changes (original {0}, steady {1}): {2}" -f $o.changes, $s.changes, (($o.ticks -eq $s.ticks) -and ($o.changes -eq $s.changes))
    "Original keeps the slowdowns (some ticks cost over twice the typical, up to {0} cycles): {1}" -f $o.max, (($o.slow -gt 0) -and ($o.max -gt 9740))
    "Steady: every tick costs exactly the typical 4870 cycles (min {0}, max {1}): {2}" -f $s.min, $s.max, (($s.min -eq 4870) -and ($s.max -eq 4870) -and ($s.slow -eq 0))
    "the average pace is kept (ticks a second original {0:N1}, steady {1:N1}, within 3 %): {2}" -f ($o.ticks / $o.secs), ($s.ticks / $s.secs), ([math]::Abs(($o.ticks / $o.secs) - ($s.ticks / $s.secs)) / ($o.ticks / $o.secs) -lt 0.03)
}
