"""TASK-033-4b: the complete ordered list of the original's drawing/timing calls in a window of the attract loop WITH their cycles (development tool; original data from scratch/).
Writes scratch/events_<name>.json = {"t0": cycle of JSR $52BB, "events": [[offset from t0, kind, params...]]}. Kinds: lores_fill, clear, hline (x0, x1, y, $1D, $1E), vline (x, y0, y1),
blit (data address, column, row, width, rows), dispatch (image address, x, y, $1D, $1E), mclear (mask table, x, y), tile (table, x, y), dot (x, y), box (index), img12x3 (address, column, row),
bigsprite, shifted (image address, x, y), delay (X, Y), poll, switch (bits).   Usage: python -I tools/capture_events.py <name> <start offset from t0> <end offset from t0>"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
import explore

NAME, A, B = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
mc = make_machine()
step = mc.cpu.step
st = {'t0': None, 'sw': None}
ev = []


def sw():
    s = mc.switches
    return (1 if s['text'] else 0) | (2 if s['mixed'] else 0) | (4 if s['page2'] else 0) | (8 if s['hires'] else 0)


def hook():
    c = mc.cpu
    m = mc.m
    pc = c.pc
    if st['t0'] is None:
        if pc == 0x52BB and m[0xE8] == 1:
            st['t0'] = c.cycles
    else:
        off = c.cycles - st['t0']
        if A <= off <= B:
            cur = sw()
            if cur != st['sw']:
                ev.append([off, 'switch', cur])
                st['sw'] = cur
            x = m[0x0C] | m[0x0D] << 8
            if pc == 0x52BB: ev.append([off, 'lores_fill'])
            elif pc == 0x6F46: ev.append([off, 'clear'])
            elif pc == 0x5CF0: ev.append([off, 'hline', x, m[0x0E] | m[0x0F] << 8, m[0x10], m[0x1D], m[0x1E]])
            elif pc == 0x5D3D: ev.append([off, 'vline', x, m[0x10], m[0x11]])
            elif pc == 0x55A6: ev.append([off, 'blit', (m[0x55B2] | m[0x55B3] << 8) + m[0x03], m[0x04], m[0x10], m[0x1A], m[0x18]])
            elif pc == 0x6A20: ev.append([off, 'dispatch', c.a | c.y << 8, x, m[0x10], m[0x1D], m[0x1E]])
            elif pc == 0x69A1: ev.append([off, 'mclear', m[0x69B8] | m[0x69B9] << 8, x, m[0x10]])
            elif pc == 0x693A: ev.append([off, 'tile', m[0x6951] | m[0x6952] << 8, x, m[0x10]])
            elif pc == 0x6172: ev.append([off, 'dot', x, m[0x10]])
            elif pc == 0x696A: ev.append([off, 'box', m[0x50]])
            elif pc == 0x6EE2: ev.append([off, 'img12x3', c.a | c.y << 8, m[0x04], m[0x10]])
            elif pc == 0x8259: ev.append([off, 'bigsprite', m[0x60], m[0x61]])
            elif pc == 0x7E1F: ev.append([off, 'shifted', m[0x16] | m[0x17] << 8, x, m[0x10], m[0x1D], m[0x1E]])
            elif pc == 0x6CE8: ev.append([off, 'delay', c.x, c.y])
            elif pc == 0x6F5E: ev.append([off, 'poll'])
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
try:
    while st['t0'] is None or mc.cpu.cycles - st['t0'] < B:
        mc.run(500_000)
except Exception as e:
    print('stopped', type(e).__name__)
json.dump({'t0': st['t0'], 'events': ev}, open('scratch/events_%s.json' % NAME, 'w'))
import collections
print('t0', st['t0'], len(ev), 'events', dict(collections.Counter(e[1] for e in ev)))
