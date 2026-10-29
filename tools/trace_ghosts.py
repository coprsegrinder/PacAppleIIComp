"""Log every ghost 'next move' decision of the routine at $8C50 during play (development tool; uses original data from scratch/).
Usage: python -I tools/trace_ghosts.py [games] [seed]      -> prints a summary, writes scratch/ghost_events.json
An event is recorded when the routine is entered; if it goes on to call a direction routine ($8CA6) the chosen routine and result are added.
Variables (all from reading the code; names are mine): $73 junction type, $E0 ghost number 1-4, $78/$E9 mode flags, $79 special flag,
$72 current direction, $70/$71 the ghost's position, $60/$61 Pac-Man's position(?), $75/$76 move-queue pointer, $77 queue index.
"""
import json, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from autoplay import start_game, KEYSET

ENTRY, DECIDE, AFTER_CALL = 0x8C50, 0x8C6C, 0x8CA9


def install(mc):
    """Hook cpu.step. Returns the (growing) list of events."""
    events = []
    current = [None]
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        m = mc.m
        if pc == ENTRY:
            current[0] = {'cycle': mc.cpu.cycles, 'type73': m[0x73], 'ghost': m[0xE0], 'f78': m[0x78], 'f79': m[0x79], 'fE9': m[0xE9],
                          'dir72': m[0x72], 'pos70': m[0x70], 'pos71': m[0x71], 'pac60': m[0x60], 'pac61': m[0x61],
                          'qidx': m[0x77], 'decided': False}
            events.append(current[0])
        elif pc == DECIDE and current[0] is not None:
            current[0]['decided'] = True
        elif pc == AFTER_CALL and current[0] is not None:
            current[0]['routine'] = m[0x8CA7] | m[0x8CA8] << 8
            current[0]['result'] = mc.cpu.a
        return step()
    mc.cpu.step = hooked
    return events


def routine_number(address, table_words):
    """Index of `address` in the 48-entry routine table, or None."""
    return table_words.index(address) if address in table_words else None


def check_dispatch(events, index_table, table_words):
    """For decided events with a routine: does routine == table_words[index_table[type*4 + ghost - 1]]? Returns (checked, mismatches)."""
    checked = bad = 0
    for e in events:
        if e.get('routine') is None or not e['decided']:
            continue
        i = e['type73'] * 4 + e['ghost'] - 1
        if i >= len(index_table):
            continue
        checked += 1
        if table_words[index_table[i]] != e['routine']:
            bad += 1
    return checked, bad


def main():
    games = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    rng = random.Random(seed)
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    events = install(mc)
    for g in range(games):
        start_game(mc)
        done = 0
        while done < 60_000_000 and mc.m[0xE8] != 1:
            if rng.random() < 0.5:
                mc.push_key(rng.choice(KEYSET))
            try:
                mc.run(150_000)
            except NeedKey:
                mc.push_key(0x0D)
            done += 150_000
        mc.run(2_000_000)
    index_table = body[0x8CBE - 0x4000:0x8CBE - 0x4000 + 256]
    words = [body[0x8DBE - 0x4000 + 2 * i] | body[0x8DBF - 0x4000 + 2 * i] << 8 for i in range(48)]
    json.dump(events, open('scratch/ghost_events.json', 'w'))
    dec = [e for e in events if e['decided']]
    print('entries to $8C50:', len(events), '| reached the decision code:', len(dec))
    print('dispatch check (routine == table[type*4+ghost-1]): checked %d, mismatches %d' % check_dispatch(events, index_table, words))


if __name__ == '__main__':
    main()
