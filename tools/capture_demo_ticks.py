"""TASK-033-6c: the cycle of every tick of the attract game after a real game, with $E5/$E6 (development tool; original data from scratch/, real monitor ROM). Same game as
capture_demo_after_game.py (<N> players, rank <L>, random keys, lives cut to one) up to the demo; then every $7861 call with $E8 = 1 is recorded.
Writes scratch/demo_ticks_<N>_<L>.txt: one line per tick: cycle, E5, E6, $59.   Usage: python -I tools/capture_demo_ticks.py <N> [L]"""
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

N = int(sys.argv[1])
L = int(sys.argv[2]) if len(sys.argv) > 2 else 1
mc = make_machine(real_rom=True)
step = mc.cpu.step
st = {'game': False, 'ticks': 0, 'rows': [], 'started': False}
rng = random.Random(7)
KEYS = [0xC1, 0xDA, 0x88, 0x95]


def hook():
    pc = mc.cpu.pc
    if pc == 0x7861:
        if mc.m[0xE8] == 0:
            st['game'] = True
            st['ticks'] += 1
            if st['ticks'] % 7 == 0:
                mc.push_key(rng.choice(KEYS))
            if st['ticks'] % 400 == 5:
                mc.m[0x58] = 0x81
        elif st['game'] and mc.m[0xE8] == 1:
            st['started'] = True
            st['rows'].append((mc.cpu.cycles, mc.m[0xE5], mc.m[0xE6], mc.m[0x59]))
    if pc == 0x52BB and st['started'] and len(st['rows']) > 100:
        open('scratch/demo_ticks_%d_%d.txt' % (N, L), 'w').write('\n'.join('%d %d %d %d' % r for r in st['rows']) + '\n')
        print('ticks', len(st['rows']))
        raise SystemExit
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
mc.run(70_000_000)
explore.send(mc, [' '])
try:
    mc.run(20_000_000)
except NeedKey:
    pass
explore.send(mc, [str(N)])
try:
    mc.run(20_000_000)
except NeedKey:
    pass
explore.send(mc, ['\r'])
try:
    for _ in range(4000):
        mc.run(1_000_000)
except SystemExit:
    pass
