"""TASK-034-TIMING: where the cycles of every demo tick go, on the oracle (development tool; original data from scratch/).
Replays the attract game (as tools/record_demo_full.py) and, per tick (from one execution of $7861 with $E8 <> 0 to the next), adds up the INCLUSIVE cycles of every subroutine called
directly from the main loop (the JSR targets at call depth 0) and the cycles outside any such call ("loop"), and the number of calls. Writes scratch/tickprofile.json:
{"cycles": [per tick], "prof": [{"target hex": [calls, cycles], ...} per tick]}.
Usage: python -I tools/tick_profile.py [ticks]       (about 4 minutes)"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import do_step
from cov_close import factory

TICK_PC = 0x7861
LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
DEPTH = int(sys.argv[2]) if len(sys.argv) > 2 else 1
mc = factory()
for st in ('key:' + os.environ.get('PMODE', '1'), 'key:cr'):
    do_step(mc, st, lambda *_: None)
cpu, m = mc.cpu, mc.m
step = cpu.step
cur = {}
ticks = []
state = {'start': None, 'depth': 0, 'stack': [], 'last': 0}


def add(key, calls, cyc):
    e = cur.setdefault(key, [0, 0])
    e[0] += calls
    e[1] += cyc


def hook():
    pc = cpu.pc
    if pc == TICK_PC and m[0xE8] != 0:
        if state['start'] is not None:
            ticks.append((cpu.cycles - state['start'], cur.copy()))
        cur.clear()
        state['start'] = cpu.cycles
        state['stack'] = []
        state['last'] = cpu.cycles
    if state['start'] is None:
        return step()
    op = m[pc]
    c0 = cpu.cycles
    r = step()
    if op == 0x20:                       # JSR
        target = cpu.pc
        st = state['stack']
        if not st:
            add('loop', 0, c0 - state['last'])      # main loop cycles since the last return, up to and including nothing of the call
            st.append((target, c0))
        else:
            st.append((target, c0))
    elif op == 0x60 and state['stack']:  # RTS
        st = state['stack']
        target, t0 = st.pop()
        if len(st) <= DEPTH - 1:
            add('>'.join(['%04X' % x[0] for x in st] + ['%04X' % target]), 1, cpu.cycles - t0)
        if not st:
            state['last'] = cpu.cycles
    return r


cpu.step = hook
while True:
    try:
        mc.run(150_000)
    except NeedKey:
        mc.push_key(13)
    if len(ticks) >= LIMIT or (ticks and cpu.cycles - state['start'] > 8_000_000):
        break
json.dump({'cycles': [t[0] for t in ticks], 'prof': [t[1] for t in ticks]}, open('scratch/tickprofile.json', 'w'))
print('ticks', len(ticks))
