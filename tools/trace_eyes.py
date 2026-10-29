"""Make one ghost 'eyes' on the oracle and watch its route back to the house (development tool; uses original data from scratch/).
Sets the eyes flag ($79 = offset 9 of the ghost's record) between main-loop passes, then logs every decision entry for that ghost:
junction $73, the fixed direction the game's table ($A360, indexed by junction) holds, and what the ghost does next.
Usage: python -I tools/trace_eyes.py [ghost 1-4] [instructions_before] [instructions_after]
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine, do_step
from autoplay import start_game
import trace_ghosts as tg

RECORD = (0x84, 0x98, 0xAC, 0xC0)


def make_eyes(mc, ghost):
    """Set the eyes flag in the ghost's stored record (call between passes, with $E0 not in use)."""
    base = RECORD[ghost - 1]
    mc.m[base + 9] = 1
    mc.m[base + 8] = 0


def route(events, ghost, table):
    """[(junction, table direction, incoming direction)] for the eyes decisions of `ghost`."""
    return [(e['type73'], table[e['type73']], e['dir72']) for e in events if e['ghost'] == ghost and e['f79']]


def main():
    ghost = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    before = int(sys.argv[2].replace('_', '')) if len(sys.argv) > 2 else 9_000_000
    after = int(sys.argv[3].replace('_', '')) if len(sys.argv) > 3 else 3_000_000
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    table = list(body[0xA360 - 0x4000:0xA360 - 0x4000 + 64])
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    mc.run(before)
    mc.run(10_000_000, stop_pc=0x530F)
    rec = RECORD[ghost - 1]
    print('before: ghost %d record: X=%02X Y=%02X dir=%d junction=%d in_house=%d' % (ghost, mc.m[rec], mc.m[rec + 1], mc.m[rec + 2], mc.m[rec + 3], mc.m[rec + 17]))
    events = tg.install(mc)
    make_eyes(mc, ghost)
    t0 = mc.cpu.cycles
    done_at = None
    for _ in range(after // 20_000):
        mc.run(20_000)
        if mc.m[rec + 9] == 0:
            done_at = mc.cpu.cycles
            break
    print('eyes flag cleared (arrived home): %s%s' % (done_at is not None, '' if done_at is None else ' after %.2f s' % ((done_at - t0) / 1022727)))
    print('after: ghost %d record: X=%02X Y=%02X junction=%d in_house=%d eyes=%d' % (ghost, mc.m[rec], mc.m[rec + 1], mc.m[rec + 3], mc.m[rec + 17], mc.m[rec + 9]))
    names = {0: 'down', 1: 'up', 2: 'right', 3: 'left'}
    r = route(events, ghost, table)
    print('eyes decision entries: %d (junction, table direction, incoming direction):' % len(r))
    print('  ' + '  '.join('j%d:%s(in %s)' % (j, names[d], names.get(i, i)) for j, d, i in r))


if __name__ == '__main__':
    main()
