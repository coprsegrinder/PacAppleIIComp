"""TASK-029h: how many CPU cycles does one tick (one pass of the main loop) cost in the original? (development tool; original data from scratch/)
Replays scenario A (keys of the golden trace) and records the cycle counter at every tick start. Writes scratch/tick_cycles_A.bin (uint32 per tick) and prints
the mean, minimum, maximum and the means per 500 ticks.   Usage: python -I tools/tick_cycles.py [ticks]   (about 2-3 minutes)"""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
import cov_close

N = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A']
G.SCENARIOS['A']['ticks'] = N
cyc = []
orig = G.state_bytes


def rec(m):
    cyc.append(cov_close.MACHINES[-1].cpu.cycles)
    return orig(m)


G.state_bytes = rec
G.run('A', stored['inputs'], stored['pokes'])
d = [b - a for a, b in zip(cyc, cyc[1:])]
open('scratch/tick_cycles_A.bin', 'wb').write(b''.join(struct.pack('<I', x) for x in d))
print('ticks', len(d), 'mean %.1f' % (sum(d) / len(d)), 'min', min(d), 'max', max(d))
for i in range(0, len(d), 500):
    c = d[i:i + 500]
    print(i, '%.1f' % (sum(c) / len(c)))
