"""TASK-034-TIMING: the cost of every call of the per-ghost routine $8763 on the oracle, split into its own code and its subroutines (development tool; original data from scratch/).
Replays the attract game; for every call of $8763 (from $86EA) records the ghost number ($E0), the cycles of the routine's own instructions (without the JSR'd subroutines), the path (the
sequence of branch outcomes of its own code) and the inclusive cycles of each JSR'd subroutine. Writes scratch/ghostcalls.json: [[ghost, self, path, {callee: cycles}], ...].
Usage: python -I tools/ghost_call_profile.py [ticks]       (about 4 minutes)"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import do_step
from apple2 import NeedKey
from cov_close import factory

LIMIT = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
mc = factory()
for st in ('key:1', 'key:cr'):
    do_step(mc, st, lambda *_: None)
cpu, m = mc.cpu, mc.m
step = cpu.step
rec = []
ticks = [0]
S = {'in': False, 'depth': 0, 'self': 0, 'path': [], 'callees': {}, 'stack': [], 'ghost': 0}
BR = {0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0}


def hook():
    pc = cpu.pc
    if pc == 0x7861 and m[0xE8] != 0:
        ticks[0] += 1
    if not S['in']:
        if pc == 0x8763:
            S.update({'in': True, 'self': 0, 'path': [], 'callees': {}, 'stack': [], 'ghost': m[0xE0], 'c_in': cpu.cycles, 'draw': 0})
        else:
            return step()
    op = m[pc]
    c0 = cpu.cycles
    r = step()
    dc = cpu.cycles - c0
    if op == 0x20:
        S['stack'].append((cpu.pc, c0))
        S['self'] += 6                          # the JSR instruction itself
    elif op == 0x60:
        if S['stack']:
            t, t0 = S['stack'].pop()
            if t in (0x6A20, 0x699E, 0x6172, 0x5CF0) and not any(x in (0x6A20, 0x699E, 0x6172, 0x5CF0) for x, _ in S['stack']):
                S['draw'] += cpu.cycles - t0                  # a drawing routine at any depth (the dot restore $6172 is called from $895F, the door lines $5CF0 from $8A7D)
            if not S['stack']:
                S['callees']['%04X' % t] = S['callees'].get('%04X' % t, 0) + cpu.cycles - t0 - 6 + 6
                S['self'] += 6 - 6               # (RTS counted inside the callee)
        else:
            S['self'] += dc
            rec.append([S['ghost'], S['self'], ''.join(S['path']), S['callees'], cpu.cycles - S['c_in'], S['draw'], ticks[0]])
            S['in'] = False
    elif not S['stack']:
        S['self'] += dc
        if op in BR:
            S['path'].append('T' if dc > 2 else 'N')
        elif op in (0x4C, 0x6C):
            S['path'].append('J%04X' % cpu.pc)
    return r


cpu.step = hook
last = 0
while True:
    try:
        mc.run(150_000)
    except NeedKey:
        mc.push_key(13)
    if ticks[0] >= LIMIT or (ticks[0] and cpu.cycles - last > 8_000_000 and False):
        break
    if ticks[0] and len(rec) > 31000:
        break
json.dump(rec, open('scratch/ghostcalls.json', 'w'))
json.dump([[r[0], r[4], r[5], r[6]] for r in rec], open('scratch/ghostcalls_total.json', 'w'))   # [ghost, entry-to-RTS cycles, of which drawing routines ($6A20, $699E), tick index]
print('calls', len(rec))
