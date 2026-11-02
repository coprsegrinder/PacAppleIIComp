"""TASK-029g2: list the drawing calls the original makes while it builds the first play screen (from the page clear $6F46 at the start of $5659 to the first
tick). Development tool; original data from scratch/. Reuses the Collector of tools/capture_hires_vectors.py (types as there: 2 = OR blit, 3 = masked clear,
4 = box clear, 5 = small dot, 6 = tile, 7 = 12x3 image, 1 = 3x10 image dispatcher) and adds the line/plot routines. Writes scratch/screen_calls.txt.
Usage: python -I tools/capture_screen_calls.py   (about 1 minute)"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import capture_hires_vectors as H
from apple2 import NeedKey
from autoplay import start_game
from play import do_step
from cov_close import factory

class Always(set):
    def __contains__(self, k): return False
    def __len__(self): return 0

mc = factory()
col = H.Collector(mc)
col.seen = {t: Always() for t in range(1, 10)}
H.LIMIT = 10 ** 9
inner = col.hook_factory()
extra = []
state = {'on': False, 'done': False}
EXTRA = {0x5CF0: 'hline', 0x5CF6: 'plot_xy', 0x5913: 'plot_inner', 0x5D3D: 'vline', 0x6F46: 'page_clear', 0x70CC: 'score_draw', 0x70D8: 'score_digits', 0x5159: 'x'}

def hook():
    pc = mc.cpu.pc
    if pc == 0x6F46 and not state['done']:
        state['on'] = True
    if state['on'] and not state['done'] and pc in (0x5CF0, 0x5CF6, 0x6F46, 0x70CC, 0x696A, 0x7159, 0x5659, 0x550A, 0x56CF, 0x5695):
        m = mc.m
        extra.append((len(col.records) + len(col.pending), '%04X' % pc, 'A=%02X X=%02X Y=%02X  $0C/D=%02X%02X $0E/F=%02X%02X $10=%02X $11=%02X $04=%02X $50=%02X' % (mc.cpu.a, mc.cpu.x, mc.cpu.y, m[0x0C], m[0x0D], m[0x0E], m[0x0F], m[0x10], m[0x11], m[0x04], m[0x50])))
    if pc == 0x7861 and mc.m[0xE8] == 0 and state['on']:
        state['done'] = True
        raise SystemExit
    return inner()
mc.cpu.step = hook
for st in ('key:1', 'key:cr', '70_000_000'):
    do_step(mc, st, lambda *_: None)
import os as _os
if _os.environ.get('PLAYERS') == '2':
    for st in ('key:sp', '3_000_000', 'key:2', '3_000_000', 'key:cr', '1_000_000'):
        do_step(mc, st, lambda *_: None)
else:
    start_game(mc)
try:
    for _ in range(80):
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
except SystemExit:
    pass
lines = []
for at, pc, txt in extra:
    lines.append('%5d  call %s  %s' % (at, pc, txt))
for i, r in enumerate(col.records):
    lines.append('%5d  type %d  %s' % (i, r[0], r[1]))
lines.sort(key=lambda s: (int(s.split()[0]), 0 if 'call' in s else 1))
open('scratch/screen_calls%s.txt' % ('_2p' if _os.environ.get('PLAYERS') == '2' else ''), 'w').write('\n'.join(lines))
print(len(col.records), 'drawing calls,', len(extra), 'routine entries')
