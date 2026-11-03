"""TASK-033-1c: per-tick cycle cost AND what each tick does, on the oracle (development tool; original data from scratch/).
For a stored scenario (A, B, C, D) replays the golden keys/pokes and records, per tick: the cycles the tick cost and how often each drawing/logic routine was entered.
Writes scratch/tickcost_<S>.json = {"cycles": [...], "names": [...], "counts": [[...] per tick]}.   Usage: python -I tools/tick_cost_record.py A [ticks]   (2-4 minutes)"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
import cov_close

S = sys.argv[1]
stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][S]
if len(sys.argv) > 2:
    G.SCENARIOS[S]['ticks'] = int(sys.argv[2])
# routine entries: drawing blitters, the dispatcher with its call type, and the main per-tick logic routines
WATCH = {0x55A6: 'or_blit', 0x69A1: 'masked_clear', 0x696A: 'box_clear', 0x693A: 'tile', 0x6172: 'dot', 0x6EE2: 'image12x3', 0x8259: 'big_sprite',
         0x8180: 'vline', 0x8173: 'hline', 0x7861: 'pacman', 0x86EA: 'ghosts', 0x6A20: 'dispatch'}
names = sorted(set(WATCH.values())) + ['type%d' % t for t in range(1, 13)]
idx = {n: i for i, n in enumerate(names)}
cycles, counts = [], []
cur = [0] * len(names)
state = {'hooked': False}
orig = G.state_bytes


def rec(m):
    mc = cov_close.MACHINES[-1]
    if not state['hooked']:
        state['hooked'] = True
        step = mc.cpu.step

        def hook():
            pc = mc.cpu.pc
            n = WATCH.get(pc)
            if n:
                cur[idx[n]] += 1
                if pc == 0x6A20:
                    t = mc.cpu.a
                    if 1 <= t <= 12:
                        cur[idx['type%d' % t]] += 1
            return step()
        mc.cpu.step = hook
    cycles.append(mc.cpu.cycles)
    counts.append(list(cur))
    for i in range(len(cur)):
        cur[i] = 0
    return orig(m)


G.state_bytes = rec
G.run(S, stored['inputs'], stored['pokes'])
# counts[t] = routine entries between state sample t-1 and t; cost[t] = cycles of that same interval
cost = [b - a for a, b in zip(cycles, cycles[1:])]
json.dump({'cycles': cost, 'names': names, 'counts': counts[1:]}, open('scratch/tickcost_%s.json' % S, 'w'))
print(S, 'ticks', len(cost), 'mean %.0f' % (sum(cost) / len(cost)))
