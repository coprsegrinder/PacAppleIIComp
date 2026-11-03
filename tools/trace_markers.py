"""TASK-033-4: iteration boundaries of the attract loop (development tool; original data from scratch/).
Logs the cycle of selected call targets and the first/last tick ($7861) of each demo-game block. Usage: python -I tools/trace_markers.py <instructions>"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

N = int(sys.argv[1])
MARK = {0x52BB: 'title_setup', 0x531E: 'game_setup_a', 0x5659: 'game_screen', 0x53E6: 'start_of_life', 0x4821: 'mode_menu', 0x54D4: 'new_game_state', 0x6F46: 'page_clear', 0x43D2: 'attract_entry', 0x4338: 'text_glyph_row', 0x467C: 'ghost_eaten_anim'}
mc = make_machine()
ev = []
step = mc.cpu.step
tick = {'first': None, 'last': None, 'n': 0}


def flush():
    if tick['n']:
        ev.append((tick['first'], 'DEMO_TICKS', '%d ticks until %d' % (tick['n'], tick['last'])))
        tick.update(first=None, last=None, n=0)


def hook():
    c = mc.cpu
    pc = c.pc
    if pc == 0x7861 and mc.m[0xE8] == 1:
        if tick['last'] is not None and c.cycles - tick['last'] > 2_000_000:
            flush()
        if tick['first'] is None:
            tick['first'] = c.cycles
        tick['last'] = c.cycles
        tick['n'] += 1
    elif mc.m[pc] == 0x20:
        tgt = mc.m[pc + 1] | mc.m[pc + 2] << 8
        if tgt in MARK and tgt not in (0x4338,):
            flush()
            ev.append((c.cycles, MARK[tgt], 'from $%04X E8=%d' % (pc, mc.m[0xE8])))
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
try:
    mc.run(N)
except NeedKey:
    ev.append((mc.cpu.cycles, 'KEY WAIT', ''))
flush()
for c, n, s in ev:
    print('%11d  %-16s %s' % (c, n, s))
print('end', mc.cpu.cycles)
