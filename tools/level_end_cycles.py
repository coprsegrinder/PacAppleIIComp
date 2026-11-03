"""TASK-033-1b: how many CPU cycles does the level end cost in the original? (development tool; original data from scratch/)
Same set-up as tools/record_levelend.py (level poked, one item left, random keys), but records the cycle counter at every tick and prints the tick-to-tick
cycle cost around the level change. Usage: python -I tools/level_end_cycles.py <level> [ticks]   (30 s to a few minutes)"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
import cov_close

level = int(sys.argv[1])
ticks = int(sys.argv[2]) if len(sys.argv) > 2 else 400
name = 'E%02d' % level
G.SCENARIOS[name] = dict(seed=300 + level, ticks=ticks, pokes=[])
inputs = G.make_inputs(300 + level, ticks)
pokes = [[5, 0x55, level], [100, 0x56, 1]] + [[t, 0x58, 0x83] for t in range(100, ticks, 100)]
if os.environ.get('GAMEOVER'):   # one life, no top-up, no item poke: random play until the last life is lost
    pokes = [[5, 0x58, 0x81]]
cyc, lev = [], []
orig = G.state_bytes


def rec(m):
    cyc.append(cov_close.MACHINES[-1].cpu.cycles)
    b = orig(m)
    lev.append(b[3])
    return b


G.state_bytes = rec
G.run(name, inputs, pokes)
d = [b - a for a, b in zip(cyc, cyc[1:])]
change = [t for t in range(1, len(lev)) if lev[t] != lev[t - 1]]
print('level', level, 'GAMEOVER run' if os.environ.get('GAMEOVER') else '', 'ticks', len(cyc), 'level changes at', change)
for i, x in sorted(enumerate(d), key=lambda p: -p[1])[:6]:
    print('  tick', i, '->', i + 1, 'cost', x, 'cycles = %.3f s' % (x / 1022727.0), 'level byte', lev[i], '->', lev[i + 1])
