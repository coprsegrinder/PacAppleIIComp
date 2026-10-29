"""Trace the ghost house release at the start of a life (development tool; uses original data from scratch/).
Watches, for each ghost record at $84/$98/$AC/$C0: offset 17 ($81 in-house flag), offset 18 ($82 house countdown), offset 8/9 ($78/$79),
and logs every change with the cycle count since the game started. Usage: python -I tools/trace_house.py [instructions]
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine, do_step
from autoplay import start_game

RECORD = (0x84, 0x98, 0xAC, 0xC0)
FIELDS = {17: 'in_house($81)', 18: 'countdown($82)', 8: 'frightened($78)', 9: 'eyes($79)'}


def install(mc):
    """Hook cpu.step: log changes of the watched fields of the four ghost records. Returns the event list."""
    events = []
    last = {(g, off): mc.m[RECORD[g] + off] for g in range(4) for off in FIELDS}
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        cycles = step()
        for (g, off), old in list(last.items()):
            v = mc.m[RECORD[g] + off]
            if v != old:
                events.append({'cycle': mc.cpu.cycles, 'ghost': g + 1, 'field': FIELDS[off], 'old': old, 'new': v, 'pc': pc})
                last[(g, off)] = v
        return cycles
    mc.cpu.step = hooked
    return events


def main():
    n = int(sys.argv[1].replace('_', '')) if len(sys.argv) > 1 else 12_000_000
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    print('records right after the game starts: countdown $82 per ghost =', [mc.m[RECORD[g] + 18] for g in range(4)],
          '| in-house $81 =', [mc.m[RECORD[g] + 17] for g in range(4)], '| level $55 =', mc.m[0x55])
    events = install(mc)
    t0 = mc.cpu.cycles
    mc.run(n)
    release = [e for e in events if e['field'].startswith('in_house')]
    print('in-house flag changes (ghost, old->new, seconds of Apple time since start):')
    for e in release:
        print('  ghost %d  %d->%d  at %.2f s  pc=%04X' % (e['ghost'], e['old'], e['new'], (e['cycle'] - t0) / 1022727, e['pc']))
    decs = {g: sum(1 for e in events if e['ghost'] == g and e['field'].startswith('countdown') and e['new'] == e['old'] - 1) for g in range(1, 5)}
    print('countdown decrements seen per ghost:', decs)
    print('other changes (first 12):')
    for e in [e for e in events if not e['field'].startswith(('in_house', 'countdown'))][:12]:
        print('  ghost %d %s %d->%d at %.2f s' % (e['ghost'], e['field'], e['old'], e['new'], (e['cycle'] - t0) / 1022727))


if __name__ == '__main__':
    main()
