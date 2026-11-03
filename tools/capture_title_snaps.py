"""TASK-033-4b: snapshots of the screen memory of the original during one attract turn (development tool; original data from scratch/; the outputs stay local).
Every STEP cycles from T0 (the cycle of JSR $52BB, found while running) the video switches, hi-res page 1 ($2000-$3FFF), text/lo-res page 1 ($0400-$07FF) and page 2 ($0800-$0BFF)
are saved; every change of a video switch is logged with its cycle.  Format of scratch/title_snaps.bin: 'TSN1', u32 count, u32 step, then per snapshot: u32 offset from T0, u8 switches
(bit0 text, bit1 mixed, bit2 page2, bit3 hires), 8192 + 1024 + 1024 bytes.   Usage: python -I tools/capture_title_snaps.py <cycles after T0> <step>"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
import explore

SPAN = int(sys.argv[1])
STEP = int(sys.argv[2])
mc = make_machine()
step = mc.cpu.step
state = {'t0': None, 'next': 0, 'sw': None}
snaps, events = [], []


def sw():
    s = mc.switches
    return (1 if s['text'] else 0) | (2 if s['mixed'] else 0) | (4 if s['page2'] else 0) | (8 if s['hires'] else 0)


def hook():
    c = mc.cpu
    if state['t0'] is None:
        if c.pc == 0x52BB and mc.m[0xE8] == 1:
            state['t0'] = c.cycles
    else:
        off = c.cycles - state['t0']
        cur = sw()
        if cur != state['sw']:
            events.append((off, cur))
            state['sw'] = cur
        if off >= state['next']:
            snaps.append((off, cur, bytes(mc.m[0x2000:0x4000]), bytes(mc.m[0x400:0x800]), bytes(mc.m[0x800:0xC00])))
            state['next'] += STEP
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
try:
    while state['t0'] is None or mc.cpu.cycles - state['t0'] < SPAN:
        mc.run(500_000)
except Exception as e:
    print('stopped', type(e).__name__)
with open('scratch/title_snaps.bin', 'wb') as f:
    f.write(b'TSN1' + struct.pack('<II', len(snaps), STEP))
    for off, cur, h, t1, t2 in snaps:
        f.write(struct.pack('<IB', off, cur) + h + t1 + t2)
print('T0', state['t0'], len(snaps), 'snapshots; switch changes:')
for off, cur in events:
    print('  +%d  text=%d mixed=%d page2=%d hires=%d' % (off, cur & 1, cur >> 1 & 1, cur >> 2 & 1, cur >> 3 & 1))
