"""TASK-033-6c: what the attract game shows after a real game (the scores of the game are still in memory). Plays a game with <N> players (A's random keys, every life but one removed at tick 5 so that
it ends quickly), waits for the attract loop and dumps the hi-res page at the first tick of the demo, plus $50-$59 and $0300-$0327 (development tool; original data from scratch/).
Usage: python -I tools/capture_demo_after_game.py <N>   writes scratch/page_demo_after_<N>.bin and scratch/demo_after_<N>.txt"""
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

N = int(sys.argv[1])
mc = make_machine(real_rom=True)
step = mc.cpu.step
st = {'game': False, 'ticks': 0, 'done': False}
rng = random.Random(7)
KEYS = [0xC1, 0xDA, 0x88, 0x95]


def hook():
    if mc.cpu.pc == 0x7861:
        if mc.m[0xE8] == 0:
            st['game'] = True
            st['ticks'] += 1
            if st['ticks'] % 7 == 0:
                mc.push_key(rng.choice(KEYS))
            if st['ticks'] % 400 == 5:
                mc.m[0x58] = 0x81        # one life left for whoever plays
        elif st['game'] and mc.m[0xE8] == 1 and not st['done']:
            st['done'] = True
            open('scratch/page_demo_after_%d.bin' % N, 'wb').write(bytes(mc.m[0x2000:0x4000]))
            open('scratch/demo_after_%d.txt' % N, 'w').write('$50=%02X $51=%02X zp52-59=%s saves=%s\n' % (mc.m[0x50], mc.m[0x51], bytes(mc.m[0x52:0x5A]).hex(), bytes(mc.m[0x300:0x328]).hex()))
            print('captured: $50=%02X $51=%02X zp52-59=%s' % (mc.m[0x50], mc.m[0x51], bytes(mc.m[0x52:0x5A]).hex()))
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
    for _ in range(3000):
        mc.run(1_000_000)
except SystemExit:
    pass
