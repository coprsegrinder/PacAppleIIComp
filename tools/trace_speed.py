"""Check the Pac-Man step-delay rule ($7874-$789B) on the oracle (development tool; uses original data from scratch/).
At every reload of $59 (pc $789D) compare the stored value with  table[$7851 + min(level,15)] + (#frightened among ghosts 2,4) - (#eyes among ghosts 1,3)
(the code reads $98+8, $C0+8, $84+9, $AC+9). Energizers are injected now and then so that these flags vary.
Usage: python -I tools/trace_speed.py [seed] [max_instr]
"""
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from autoplay import start_game, KEYSET


def run(seed=3, n=40_000_000):
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    rows = []
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        if pc == 0x7ABD and mc.m[0xE8] == 0 and mc.m[0x78] == 0 and mc.m[0x56] % 8 == 0:
            mc.m[mc.m[0x16] | mc.m[0x17] << 8] = 131
        if pc == 0x789D and mc.m[0xE8] == 0:
            lvl = min(mc.m[0x55], 15)
            pred = mc.m[0x7851 + lvl] + (mc.m[0xA0] != 0) + (mc.m[0xC8] != 0) - (mc.m[0x8D] != 0) - (mc.m[0xB5] != 0)
            rows.append((mc.m[0x59], pred, (mc.m[0xA0] != 0) + (mc.m[0xC8] != 0), (mc.m[0x8D] != 0) + (mc.m[0xB5] != 0), mc.m[0xF3]))
        return step()
    mc.cpu.step = hooked
    rng, done = random.Random(seed), 0
    while done < n and mc.m[0xE8] == 0:
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
        done += 150_000
    return rows


if __name__ == '__main__':
    rows = run(int(sys.argv[1]) if len(sys.argv) > 1 else 3, int(sys.argv[2].replace('_', '')) if len(sys.argv) > 2 else 40_000_000)
    from collections import Counter
    print('reloads', len(rows), '| stored == predicted:', sum(1 for r in rows if r[0] == r[1]))
    print('(stored, fright, eyes) counts:', Counter((r[0], r[2], r[3]) for r in rows).most_common(8))
    print('mismatches:', [r for r in rows if r[0] != r[1]][:5])
