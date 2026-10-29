"""Record the speaker click timings (cycle counts of each $C030 access) from the start of a game and summarise them as constant-pitch runs.
Development tool; original data from scratch/. NOT verified against AppleWin (the emulator was not asked to record audio).
Usage: python -I tools/save_clicks.py [instructions_of_play=12_000_000]   -> scratch/clicks.json, prints a summary
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine

CPU_HZ = 1_022_727


def notes(clicks, tol=0.15, min_run=3):
    """Group clicks into runs whose spacing stays within +-tol of the run's first spacing.
    Returns [(start_cycle, count_of_clicks, spacing_cycles)]."""
    out, i = [], 0
    while i < len(clicks) - 1:
        gap = clicks[i + 1] - clicks[i]
        j = i + 1
        while j < len(clicks) - 1 and abs((clicks[j + 1] - clicks[j]) - gap) <= tol * gap:
            j += 1
        if j - i + 1 >= min_run:
            out.append((clicks[i], j - i + 1, gap))
            i = j + 1
        else:
            i += 1
    return out


def main():
    n = int(sys.argv[1].replace('_', '')) if len(sys.argv) > 1 else 12_000_000
    mc = make_machine()
    mc.push_key('1')
    mc.push_key(0x0D)
    for step in (3_000_000,):
        try:
            mc.run(step)
        except NeedKey:
            pass
    mc.push_key(' ')
    try:
        mc.run(20_000_000)
    except NeedKey:
        pass
    mc.push_key('1')
    try:
        mc.run(20_000_000)
    except NeedKey:
        pass
    mc.speaker.clear()
    t0 = mc.cpu.cycles
    mc.push_key(0x0D)
    mc.run(n)
    clicks = [c - t0 for c in mc.speaker]
    json.dump({'cpu_hz': CPU_HZ, 'clicks': clicks}, open('scratch/clicks.json', 'w'))
    runs = notes(clicks)
    print('clicks %d over %.2f s of Apple time (%d cycles)' % (len(clicks), (mc.cpu.cycles - t0) / CPU_HZ, mc.cpu.cycles - t0))
    print('constant-pitch runs: %d; first 12 (start s, clicks, spacing cycles, approx Hz of the toggled square wave):' % len(runs))
    for s, c, g in runs[:12]:
        print('  %.3f s  %4d clicks  spacing %5d  ~%.0f Hz' % (s / CPU_HZ, c, g, CPU_HZ / (2 * g)))


if __name__ == '__main__':
    main()
