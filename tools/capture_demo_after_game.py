"""TASK-033-6c: what the attract game shows and does after a real game (the last game's players, scores, high score and level are still in memory). Plays a game with <N> players at the
first level <L> (the rank of the hidden menu), with random keys; the lives of whoever plays are cut to one every 400 ticks so that the game ends quickly. When the attract loop is back it
dumps the hi-res page at the first tick of the demo, the memory the demo starts from, and then follows the turn: the number of demo ticks, the cycle of the first tick, of the
death routine ($8BDD) and of the next turn's $52BB (development tool; original data from scratch/, real monitor ROM).
Usage: python -I tools/capture_demo_after_game.py <N> [L]   writes scratch/page_demo_after_<N>_<L>.bin and scratch/demo_after_<N>_<L>.txt"""
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

N = int(sys.argv[1])
L = int(sys.argv[2]) if len(sys.argv) > 2 else 1
mc = make_machine(real_rom=True)
step = mc.cpu.step
st = {'game': False, 'ticks': 0, 'phase': 0, 'dticks': 0, 'first': None, 'death': None, 't0': None}
rng = random.Random(7)
KEYS = [0xC1, 0xDA, 0x88, 0x95]
info = {}


def hook():
    pc = mc.cpu.pc
    if pc == 0x7861:
        if mc.m[0xE8] == 0:
            st['game'] = True
            st['ticks'] += 1
            if st['ticks'] % 7 == 0:
                mc.push_key(rng.choice(KEYS))
            if st['ticks'] % 400 == 5:
                mc.m[0x58] = 0x81        # one life left for whoever plays
        elif st['game'] and mc.m[0xE8] == 1:
            if st['phase'] == 0:
                st['phase'] = 1
                st['first'] = mc.cpu.cycles
                open('scratch/page_demo_after_%d_%d.bin' % (N, L), 'wb').write(bytes(mc.m[0x2000:0x4000]))
                open('scratch/mem_demo_after_%d_%d.bin' % (N, L), 'wb').write(bytes(mc.m[0:0x400]))   # zero page, stack, input buffer, the saves at $0300
                info['start'] = 'players=%d current=%d live=%s saves=%s high=%s level_icon=%02X' % (mc.m[0x51], mc.m[0x50], bytes(mc.m[0x52:0x5A]).hex(), bytes(mc.m[0x300:0x328]).hex(), bytes(mc.m[0x00:0x03]).hex(), mc.m[0x55])
            st['dticks'] += 1
    if pc == 0x52BB and st['phase'] == 0 and mc.m[0xE8] == 1:
        st['t0'] = mc.cpu.cycles                  # the start of the attract turn that leads to this demo
    if pc == 0x8BDD and st['phase'] == 1 and mc.m[0xE8] == 1 and st['death'] is None:
        st['death'] = mc.cpu.cycles
        open('scratch/mem_demo_after_%d_%d_death.bin' % (N, L), 'wb').write(bytes(mc.m[0:0x400]))
    if pc == 0x52BB and st['phase'] == 1 and st['death'] is not None:
        open('scratch/demo_after_%d_%d.txt' % (N, L), 'w').write('%s\nticks=%d first=%d death=%d next=%d t0=%d\n' % (info['start'], st['dticks'], st['first'], st['death'], mc.cpu.cycles, st['t0']))
        print('captured', N, L, info['start'][:60], 'ticks', st['dticks'], 'first->death', st['death'] - st['first'], 'first->next', mc.cpu.cycles - st['first'])
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
if L > 1:   # the hidden menu: Ctrl-V, Ctrl-C, a line that is not C, the rank
    for keys in ([0x16], [0x03], list('X') + ['\r'], list(str(L)) + ['\r']):
        explore.send(mc, keys)
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
