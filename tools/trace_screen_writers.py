"""TASK-027a: which instructions write to the screen memory, and which video soft switches the game uses (development tool; original data from scratch/).
Scenario: boot -> '1' + Return -> attract (70M instr) -> a game with random keys, a forced level end, and the game going on in the next level.
Records, for each writing instruction (pc): number of writes, lowest/highest address, per screen area ($0400 text 1, $0800 text 2, $2000 hi-res 1,
$4000 hi-res 2 = inside the program, so it must stay 0); and every distinct (pc, new switch state) of $C050-$C057.
Writes scratch/screen_writers.json. Stops itself after 12 minutes. Usage: python -I tools/trace_screen_writers.py [play|cartoons|rank|custom] (default play; others use tools/explore.py scenarios)"""
import json, os, random, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory

AREAS = [('text1', 0x0400, 0x07FF), ('text2', 0x0800, 0x0BFF), ('hires1', 0x2000, 0x3FFF), ('hires2', 0x4000, 0x5FFF), ('lores_other', 0x0C00, 0x1FFF)]
START = time.monotonic()


def area_of(a):
    for n, lo, hi in AREAS:
        if lo <= a <= hi:
            return n
    return None


def main(mode='play'):
    mc = factory()
    writers, switches = {}, {}
    wr = mc.write
    io = mc.io

    def write(a, v):
        n = area_of(a)
        if n and mc.m[0xE8] >= 0:
            k = (n, mc.cur_pc)
            w = writers.setdefault(k, [0, 0xFFFF, 0])
            w[0] += 1
            w[1] = min(w[1], a)
            w[2] = max(w[2], a)
        return wr(a, v)

    def io_hook(a, v):
        if 0xC050 <= a <= 0xC057:
            k = (a, mc.cur_pc)
            switches[k] = switches.get(k, 0) + 1
        return io(a, v)
    mc.write, mc.io = write, io_hook
    level0 = mc.m[0x55]
    if mode != 'play':
        import explore
        explore.SCENARIOS[mode](mc)
    else:
        for st in ('key:1', 'key:cr', '70_000_000'):
            do_step(mc, st, lambda *_: None)
        start_game(mc)
        state, step = {'armed': False}, mc.cpu.step

        def hooked():
            if mc.cpu.pc == 0x7B15 and mc.m[0xE8] == 0 and not state['armed']:
                mc.m[0x56] = 3
                state['armed'] = True
            return step()
        mc.cpu.step = hooked
        rng, level0, extra = random.Random(2), mc.m[0x55], 0
        for _ in range(1500):
            if time.monotonic() - START > 12 * 60:
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
    out = {'writers': [[n, pc, c, lo, hi] for (n, pc), (c, lo, hi) in sorted(writers.items())],
           'switches': [[a, pc, c] for (a, pc), c in sorted(switches.items())], 'final_switches': mc.switches}
    json.dump(out, open('scratch/screen_writers%s.json' % ('' if mode == 'play' else '_' + mode), 'w'))
    for n, pc, c, lo, hi in out['writers']:
        print('%-8s pc $%04X writes %8d  $%04X-$%04X' % (n, pc, c, lo, hi))
    for a, pc, c in out['switches']:
        print('switch $%04X from pc $%04X x%d' % (a, pc, c))
    print('final', mc.switches, 'level', level0, '->', mc.m[0x55], 'elapsed %.0f s' % (time.monotonic() - START))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'play')
