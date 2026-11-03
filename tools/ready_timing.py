"""TASK-033-2: when does the original draw the READY text and how long until the game ticks? (development tool; original data from scratch/)
Replays golden scenario A (a death at tick ~1976) with a cycle-stamped hook on the OR blit $55A6 (pointer to the picture in $0E/$0F... logged with A, Y and $0E/$0F)
and on the tick start $7861. Prints every blit whose picture address is $5890 (READY) with its cycle and the cycles to the next tick starts, plus the cycle of every tick start
around it.   Usage: python -I tools/ready_timing.py [ticks]   (about 2 minutes)"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
import cov_close

ticks = int(sys.argv[1]) if len(sys.argv) > 1 else 2100
stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A']
G.SCENARIOS['A']['ticks'] = ticks
ev = []
orig_factory = G.factory


def factory():
    mc = orig_factory()
    step = mc.cpu.step

    def hook():
        pc = mc.cpu.pc
        if pc == 0x55A6:
            ev.append((mc.cpu.cycles, 'blit', mc.cpu.a, mc.cpu.y, mc.m[0x0E] | mc.m[0x0F] << 8, mc.m[0x14] | mc.m[0x15] << 8))
        elif pc == 0x7861 and mc.m[0xE8] == 0:
            ev.append((mc.cpu.cycles, 'tick', 0, 0, 0, 0))
        return step()
    mc.cpu.step = hook
    return mc


G.factory = factory
G.run('A', stored['inputs'], stored['pokes'])
import pickle
pickle.dump(ev, open('scratch/ready_events.pkl', 'wb'))
ticksAt = [e[0] for e in ev if e[1] == 'tick']
print('events', len(ev), 'ticks', len(ticksAt))
cands = [e for e in ev if e[1] == 'blit' and 0x5890 in (e[2] | e[3] << 8, e[4], e[5], e[2] << 8 | e[3])]
print('READY candidates', len(cands))
for e in cands[:12]:
    nxt = [t for t in ticksAt if t > e[0]][:1]
    prv = [t for t in ticksAt if t < e[0]][-1:]
    print('  READY blit at cycle', e[0], 'prev tick', prv, 'next tick', nxt, 'wait to next tick', (nxt[0] - e[0]) if nxt else None, 'after prev', (e[0] - prv[0]) if prv else None)
