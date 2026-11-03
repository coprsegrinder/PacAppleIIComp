"""TASK-033-4c: per-instruction cycle costs of the original in a cycle window (development tool; original data from scratch/).
Usage: python -I tools/trace_pass.py <start offset from T0> <length>   prints (pc, count, total cycles) in order of first appearance."""
import os, sys, collections
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
import explore

A, L = int(sys.argv[1]), int(sys.argv[2])
mc = make_machine()
step = mc.cpu.step
st = {'t0': None}
log = []


def hook():
    c = mc.cpu
    if st['t0'] is None:
        if c.pc == 0x52BB and mc.m[0xE8] == 1:
            st['t0'] = c.cycles
    else:
        off = c.cycles - st['t0']
        if A <= off < A + L:
            log.append((c.pc, off))
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
while st['t0'] is None or mc.cpu.cycles - st['t0'] < A + L:
    mc.run(500_000)
agg = collections.OrderedDict()
for (pc, c), (pc2, c2) in zip(log, log[1:]):
    a = agg.setdefault(pc, [0, 0])
    a[0] += 1
    a[1] += c2 - c
for pc, (n, d) in agg.items():
    print('%04X x%-3d %5d cycles  (%.1f each)' % (pc, n, d, d / n))
print('total', log[-1][1] - log[0][1])
