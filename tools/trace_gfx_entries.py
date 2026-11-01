"""TASK-027b: who calls the small drawing routines and with what parameters (development tool; original data from scratch/).
Entries: $693A tile OR, $696A box clear, $69A1 masked clear (via $69E9), $6EE2 12x3 image (fruit row, via $6F13), $6113 dot field, $6172 dot, $61B6 energizer,
$6F46 page clear, $5D53 wall draw. For each (entry, caller) it keeps the first few distinct parameter sets and the count.
Scenario as trace_screen_writers.py. Writes scratch/gfx_entries.json; stops after 8 minutes. Usage: python -I tools/trace_gfx_entries.py"""
import json, os, random, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory

ENTRIES = {0x693A: 'tile_or', 0x696A: 'box_clear', 0x69A1: 'masked_clear', 0x69E9: 'masked_clear_pair', 0x6EE2: 'image12x3', 0x6F13: 'fruit_row',
           0x6113: 'dot_field', 0x6172: 'dot', 0x61B6: 'energizer', 0x6F46: 'page_clear', 0x5D53: 'wall_draw'}
START = time.monotonic()


def main():
    mc = factory()
    log = {}
    step = mc.cpu.step

    def hook():
        pc = mc.cpu.pc
        if pc in ENTRIES:
            c, m = mc.cpu, mc.m
            sp = c.sp
            caller = ((m[0x100 + ((sp + 2) & 0xFF)] << 8 | m[0x100 + ((sp + 1) & 0xFF)]) - 2) & 0xFFFF
            par = dict(A=c.a, X=c.x, Y=c.y, col=m[4], row=m[0x10], x=m[0x0C] | m[0x0D] << 8, z50=m[0x50], lvl=m[0x55], e8=m[0xE8],
                       p69B8=m[0x69B8] | m[0x69B9] << 8, p6951=m[0x6951] | m[0x6952] << 8, p6F00=m[0x6F00] | m[0x6F01] << 8)
            e = log.setdefault('%04X<-%04X' % (pc, caller), {'name': ENTRIES[pc], 'n': 0, 'params': []})
            e['n'] += 1
            if len(e['params']) < 6 and par not in e['params']:
                e['params'].append(par)
        return step()
    mc.cpu.step = hook
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    state = {'armed': False}
    step2 = mc.cpu.step

    def hooked():
        if mc.cpu.pc == 0x7B15 and mc.m[0xE8] == 0 and not state['armed']:
            mc.m[0x56] = 3
            state['armed'] = True
        return step2()
    mc.cpu.step = hooked
    rng, level0, extra = random.Random(2), mc.m[0x55], 0
    for _ in range(1500):
        if time.monotonic() - START > 8 * 60:
            break
        if mc.m[0x55] != level0:
            extra += 1
            if extra > 100:
                break
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    json.dump(log, open('scratch/gfx_entries.json', 'w'), indent=1)
    for k, v in sorted(log.items()):
        print(k, v['name'], 'x%d' % v['n'])
        for p in v['params'][:3]:
            print('    ', p)
    print('elapsed %.0f s' % (time.monotonic() - START))


if __name__ == '__main__':
    main()
