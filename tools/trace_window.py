"""TASK-033-4: drawing events of the original in a cycle window of the attract loop (development tool; original data from scratch/).
Usage: python -I tools/trace_window.py <start_cycle> <end_cycle> ...   (several windows allowed; prints blits, clears, tiles, dispatches, page clears and key polls)"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
import explore

W = [(int(sys.argv[i]), int(sys.argv[i + 1])) for i in range(1, len(sys.argv) - 1, 2)]
NAMES = {0x69A1: 'mclear', 0x6A20: 'dispatch', 0x693A: 'tile', 0x696A: 'boxclear', 0x6EE2: 'img12x3', 0x8259: 'bigsprite', 0x55A6: 'blit', 0x6F46: 'PAGECLEAR', 0x6F2F: 'HIRES_ON', 0x6F3C: 'TEXT_ON', 0x52BB: 'lores_fill', 0x7861: 'tick', 0x6172: 'dot', 0x4A6A: 'textchar'}
mc = make_machine()
step = mc.cpu.step
out = []


def hook():
    pc = mc.cpu.pc
    n = NAMES.get(pc)
    if n:
        c = mc.cpu.cycles
        for a, b in W:
            if a <= c <= b:
                out.append((c, n, mc.cpu.a, mc.cpu.y, mc.m[0xE8]))
                break
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
end = max(b for a, b in W)
try:
    while mc.cpu.cycles < end:
        mc.run(1_000_000)
except Exception as e:
    print('stopped', type(e).__name__)
last = None
for c, n, a, y, e8 in out:
    if n in ('tick',) and last == 'tick':
        continue
    print('%11d %-10s a=%3d y=%3d E8=%d' % (c, n, a, y, e8))
    last = n
