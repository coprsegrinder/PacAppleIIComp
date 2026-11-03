"""TASK-033-4: the top-level flow of the original from the boot menu through the attract loop (development tool; original data from scratch/).
Logs every JSR at call depth <= MAXDEPTH (depth from the stack pointer) with its cycle, the keyboard waits and the demo flag $E8, so the screens and their order/timing can be read off.
Usage: python -I tools/trace_flow.py <instructions> [maxdepth]     writes scratch/flow_trace.txt"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

N = int(sys.argv[1])
MAXD = int(sys.argv[2]) if len(sys.argv) > 2 else 3
mc = make_machine()
log = []
step = mc.cpu.step
last = [None, 0]


def hook():
    c = mc.cpu
    pc = c.pc
    if mc.m[pc] == 0x20 and pc < 0xC000:
        depth = (0xFF - c.sp) // 2
        if (0x4000 <= pc < 0x4830) or (0x5290 <= pc < 0x5330) or (MAXD >= 0 and depth <= MAXD):
            tgt = mc.m[pc + 1] | mc.m[pc + 2] << 8
            key = (depth, tgt)
            if last[0] == key:
                last[1] += 1
                log[-1][3] = last[1]
                log[-1][6] = c.cycles
            else:
                last[0] = key
                last[1] = 1
                log.append([c.cycles, depth, tgt, 1, mc.m[0xE8], pc, c.cycles])
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
try:
    mc.run(N)
    print('instruction limit reached')
except NeedKey:
    print('key wait at cycle', mc.cpu.cycles)
with open('scratch/flow_trace.txt', 'w') as f:
    for cyc, d, t, n, e8, caller, lastc in log:
        f.write('%10d d%d JSR $%04X x%d  E8=%d  from $%04X  last=%d\n' % (cyc, d, t, n, e8, caller, lastc))
print(len(log), 'entries; last cycle', mc.cpu.cycles)
