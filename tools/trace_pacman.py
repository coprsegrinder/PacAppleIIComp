"""Trace Pac-Man-side events: dot/energizer eating, scoring, lives, ghost eating, death (development tool; uses original data from scratch/).
Watches zero-page $52-$54 (score, BCD), $55 level, $56/$57 (dot counters), $58 (lives), $68 (ghost score step) and the entry points
$7AC7 (energizer eaten), $7AF4 (plain dot eaten), $8B9D (collision resolved), $8BDD (Pac-Man caught), $6CF4 (level complete).
Usage: python -I tools/trace_pacman.py [mode 1|2] [seed] [instructions] [out.json]
"""
import json, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from autoplay import start_game, KEYSET

WATCH = {0x52: 'score_lo', 0x53: 'score_mid', 0x54: 'score_hi', 0x55: 'level', 0x56: 'dots_left', 0x57: 'dots_b', 0x58: 'lives', 0x68: 'ghost_step'}
MARKS = {0x7AC7: 'energizer_eaten', 0x7AF4: 'dot_eaten', 0x8B9D: 'collision_resolved', 0x8BDD: 'pacman_caught', 0x6CF4: 'level_complete'}


def install(mc):
    ev = []
    last = {a: mc.m[a] for a in WATCH}
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        if pc in MARKS:
            ev.append({'cycle': mc.cpu.cycles, 'mark': MARKS[pc], 'a': [mc.m[a] for a in sorted(WATCH)], 'ghost78': mc.m[0x78],
                       'cell': mc.m[mc.m[0x16] | mc.m[0x17] << 8] if pc in (0x7AC7, 0x7AF4) else None})
        c = step()
        for a, old in list(last.items()):
            v = mc.m[a]
            if v != old:
                ev.append({'cycle': mc.cpu.cycles, 'var': WATCH[a], 'old': old, 'new': v, 'pc': pc})
                last[a] = v
        return c
    mc.cpu.step = hooked
    return ev


def run(mode, seed, n, quiet=False):
    rng = random.Random(seed)
    mc = make_machine()
    for st in ('key:%d' % mode, 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    if not quiet:
        print('after start: score', [mc.m[a] for a in (0x54, 0x53, 0x52)], 'level', mc.m[0x55], 'lives $58', mc.m[0x58], '$56', mc.m[0x56], '$57', mc.m[0x57])
    ev = install(mc)
    done = 0
    while done < n and mc.m[0xE8] != 1:
        if rng.random() < 0.5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(0x0D)
        done += 150_000
    return ev


def main():
    mode = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    n = int(sys.argv[3].replace('_', '')) if len(sys.argv) > 3 else 60_000_000
    out = sys.argv[4] if len(sys.argv) > 4 else 'scratch/pacman_events.json'
    ev = run(mode, seed, n)
    json.dump(ev, open(out, 'w'))
    from collections import Counter
    print('events', len(ev), Counter(e.get('mark') or e['var'] for e in ev))


if __name__ == '__main__':
    main()
