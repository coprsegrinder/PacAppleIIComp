"""TASK-033-6b: the first play screen (hi-res page 1 at the first tick of a game) for N players, from the original with the real monitor ROM (development tool; original data from scratch/).
Usage: python -I tools/capture_players_screen.py <N 1-5>        writes scratch/page_players_<N>.bin (8,192 bytes) and scratch/zp_players_<N>.txt ($50, $51 and the saved player bytes $0300-$0327 at that tick)."""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

N = int(sys.argv[1])
mc = make_machine(real_rom=True)
step = mc.cpu.step
st = {'hit': False}


def hook():
    if mc.cpu.pc == 0x7861 and mc.m[0xE8] == 0 and not st['hit']:
        st['hit'] = True
        open('scratch/page_players_%d.bin' % N, 'wb').write(bytes(mc.m[0x2000:0x4000]))
        open('scratch/zp_players_%d.txt' % N, 'w').write('$50=%02X $51=%02X saves=%s\n' % (mc.m[0x50], mc.m[0x51], bytes(mc.m[0x300:0x328]).hex()))
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
    for _ in range(100):
        mc.run(1_000_000)
except SystemExit:
    print('captured', N)
