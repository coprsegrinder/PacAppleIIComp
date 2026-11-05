# TASK-034-11: real-program test of the pacing of the modern FPS modes (muted, own settings and log files in scratch/, nothing touches the desktop). The same game is played by the test aid
# (--aid-energizer: Pac-Man wanders by himself and eats a power pill about 4 s in) in Native (the original's timing) and at 60 FPS (the smoothed pace of pacing.h); the program's own log has a
# line a second. Checks: the logic is the same; the pace (ticks a second, Pac-Man's steps a second) is the original's before and after the pill; the hitches are gone in the modern mode.
# Needs scratch/local.pmpk. Run: powershell -ExecutionPolicy Bypass -File tools/test_pacing.ps1   (expects every line to end in True)
$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "scratch"
$exe = Join-Path $root "out\bat\pacman_play.exe"
$pack = Join-Path $dir "local.pmpk"
function Run($fps) {
    $ini = Join-Path $dir "test_pacing_settings.ini"; $log = Join-Path $dir "test_pacing_log.txt"
    Remove-Item $log -ErrorAction SilentlyContinue
    "version=1`r`ndisplay_mode=windowed`r`nwindow_x=60`r`nwindow_y=60`r`nwindow_w=840`r`nwindow_h=576`r`nfps=$fps`r`n" | Set-Content $ini -Encoding ASCII
    $p = Start-Process -FilePath $exe -ArgumentList "--pack `"$pack`" --level 1 --seconds 24 --aid-energizer --mute --settings `"$ini`" --log `"$log`"" -WorkingDirectory $root -PassThru
    [void]$p.WaitForExit(60000)
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    $text = Get-Content $log -Raw
    $sec = [regex]::Matches($text, "second: ticks=(\d+) mean_cost=(\d+) picture_changes=(\d+) pacman_steps=(\d+) ghost_moves=(\d+) dots_eaten=(\d+) acted=(\d+) frightened=(\d) energizers_so_far=(\d+)")
    $g = [regex]::Match($text, "game: ticks=(\d+) seconds=([\d.]+) .*ordinary_min=(\d+) ordinary_max=(\d+)")
    $rows = foreach ($m in $sec) { [pscustomobject]@{ ticks = [double]$m.Groups[1].Value; steps = [double]$m.Groups[4].Value; ghosts = [double]$m.Groups[5].Value; acted = [double]$m.Groups[7].Value; fright = [int]$m.Groups[8].Value; pills = [int]$m.Groups[9].Value } }
    $before = $rows | Where-Object { $_.fright -eq 0 -and $_.pills -eq 0 } | Select-Object -Skip 1
    $after = $rows | Where-Object { $_.fright -eq 1 } | Select-Object -Skip 1 | Select-Object -SkipLast 1
    [pscustomobject]@{
        ticksBefore = ($before | Measure-Object ticks -Average).Average; ticksAfter = ($after | Measure-Object ticks -Average).Average
        stepsBefore = ($before | Measure-Object steps -Average).Average; stepsAfter = ($after | Measure-Object steps -Average).Average
        actedBefore = ($before | Measure-Object acted -Average).Average; actedAfter = ($after | Measure-Object acted -Average).Average
        ghostsBefore = ($before | Measure-Object ghosts -Average).Average; ghostsAfter = ($after | Measure-Object ghosts -Average).Average
        gameTicks = [double]$g.Groups[1].Value; ordMin = [double]$g.Groups[3].Value; ordMax = [double]$g.Groups[4].Value
    }
}
$n = Run "native"
$m = Run "60"
foreach ($pair in @(@("native (the original's timing)", $n), @("60 FPS (smoothed pace)", $m))) {
    $r = $pair[1]
    "{0,-32} ticks a second {1:N0} -> {2:N0}; Pac-Man steps a second {3:N1} -> {4:N1} ({5:N0} %); ghost moves a second {6:N1} -> {7:N1}; ordinary tick time {8} .. {9} cycles" -f $pair[0], $r.ticksBefore, $r.ticksAfter, $r.stepsBefore, $r.stepsAfter, (100 * $r.stepsAfter / $r.stepsBefore), $r.ghostsBefore, $r.ghostsAfter, $r.ordMin, $r.ordMax
}
"Pac-Man acts a second (his step timer): native {0:N1} -> {1:N1} ({2:N0} %), 60 FPS {3:N1} -> {4:N1} ({5:N0} %)" -f $n.actedBefore, $n.actedAfter, (100 * $n.actedAfter / $n.actedBefore), $m.actedBefore, $m.actedAfter, (100 * $m.actedAfter / $m.actedBefore)
$ratioN = $n.stepsAfter / $n.stepsBefore; $ratioM = $m.stepsAfter / $m.stepsBefore
"Pac-Man keeps the original's pace when the pill is eaten (after/before: native {0:N2}, 60 FPS {1:N2}, within 0.12 of each other): {2}" -f $ratioN, $ratioM, ([math]::Abs($ratioN - $ratioM) -lt 0.12)
"the ghosts keep the original's pace too (after/before: native {0:N2}, 60 FPS {1:N2}, within 0.12): {2}" -f ($n.ghostsAfter / $n.ghostsBefore), ($m.ghostsAfter / $m.ghostsBefore), ([math]::Abs($n.ghostsAfter / $n.ghostsBefore - $m.ghostsAfter / $m.ghostsBefore) -lt 0.12)
"the average pace before the pill is the original's (ticks a second {0:N0} native, {1:N0} at 60 FPS, within 4 %): {2}" -f $n.ticksBefore, $m.ticksBefore, ([math]::Abs($n.ticksBefore - $m.ticksBefore) / $n.ticksBefore -lt 0.04)
"native has its hitches (an ordinary tick takes up to {0} cycles): {1}" -f $n.ordMax, ($n.ordMax -gt 12000)
"60 FPS has no hitches: its ordinary ticks take {0} .. {1} cycles (a spread of {2:N1} times: the pace follows the situation, the blue ghosts' shorter ticks included), native {3} .. {4} ({5:N1} times): {6}" -f $m.ordMin, $m.ordMax, ($m.ordMax / $m.ordMin), $n.ordMin, $n.ordMax, ($n.ordMax / $n.ordMin), (($m.ordMax / $m.ordMin) -lt 1.8 -and ($m.ordMax / $m.ordMin) -lt 0.4 * ($n.ordMax / $n.ordMin))
