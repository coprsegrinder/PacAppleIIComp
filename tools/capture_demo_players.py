"""TASK-033-6c: does the attract game show the number of players chosen at the prompt? Boot, attract, Space, digit N, Space (back to the demo), then dump the hi-res page at the first tick of the
demo ($7861 with $E8 = 1) and $50/$51 (development tool; original data from scratch/).  Usage: python -I tools/capture_demo_players.py <N>   writes scratch/page_demo_players_<N>.bin"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

N = int(sys.argv[1])
mc = make_machine(real_rom=True)
step = mc.cpu.step
st = {'hit': False, 'back': False}


def hook():
    if st['back'] and mc.cpu.pc == 0x7861 and mc.m[0xE8] == 1 and not st['hit']:
        st['hit'] = True
        open('scratch/page_demo_players_%d.bin' % N, 'wb').write(bytes(mc.m[0x2000:0x4000]))
        open('scratch/mem_demo_players_%d.bin' % N, 'wb').write(bytes(mc.m[0:0x400]))
        print('demo first tick: $50=%02X $51=%02X' % (mc.m[0x50], mc.m[0x51]))
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
st['back'] = True
explore.send(mc, [' '])
try:
    for _ in range(200):
        mc.run(1_000_000)
except SystemExit:
    pass
