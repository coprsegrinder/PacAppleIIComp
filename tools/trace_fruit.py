"""Fruit behaviour on the oracle (TASK-031 analysis; development tool, original data from scratch/).
Part 1 (standalone, per level 1..30): run the fruit set-up ($6A7C with $57 = 0) and the fruit-eaten routine ($6CB2) and report
  the level-indexed values it produces (sprite pointer, lifetime, score in hundreds) plus the number of speaker clicks of the eaten sound.
Part 2 (real play, random keys): log when the fruit appears (items eaten so far), how many Pac-Man ticks it stays, and force one pick-up
  by placing Pac-Man on the fruit spot, to see the score and the state change ($E7: 0 none, 1 shown, 2 score shown after eating).
Usage: python -I tools/trace_fruit.py [seed] [max_instr]   (part 2 presets $57 = 3 so that the fruit shows up early)
"""
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey, Machine
from play import make_machine, do_step
from autoplay import start_game, KEYSET


def call(mc, addr, **regs):
    m, cpu = mc.m, mc.cpu
    m[0x0300:0x0304] = bytes([0x20, addr & 255, addr >> 8, 0xEA])
    cpu.pc, cpu.sp = 0x0300, 0xFD
    mc.run(5_000_000, stop_pc=0x0303)
    assert cpu.pc == 0x0303, 'did not return from %04X' % addr


def per_level():
    mc = make_machine()
    rows = {}
    for level in range(1, 31):
        m = mc.m
        m[0x55], m[0x57], m[0xE7], m[0x59], m[0xE8] = level, 0, 0, 5, 0
        call(mc, 0x6A7C)
        setup = {'sprite_ptr': m[0x5C] | m[0x5D] << 8, 'lifetime': m[0x5A] | m[0x5B] << 8, 'E7': m[0xE7], 'next_57': m[0x57]}
        m[0x53], m[0x54], m[0x52] = 0, 0, 0
        mc.speaker.clear()
        call(mc, 0x6CB2)
        setup['score_hundreds'] = '%02X' % m[0x53] + ('+%02X' % m[0x54] if m[0x54] else '')
        setup['clicks'] = len(mc.speaker)
        rows[level] = setup
    return rows


def play(seed=1, n=60_000_000, preset57=None):
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    ev, st = [], {'forced': False, 'ticks': 0, 'score_before': None}
    step = mc.cpu.step

    def hooked():
        pc, m = mc.cpu.pc, mc.m
        if m[0xE8] == 0:
            if preset57 is not None and pc == 0x7B15 and not st.get('preset'):
                st['preset'] = True               # shortcut after the first dot: fruit after a few more instead of 64 (the rule itself is the original code)
                m[0x57] = preset57
            if pc == 0x6AA5:
                ev.append(('appears', 248 - m[0x56], m[0x55], mc.cpu.cycles))
                st['ticks'] = 0
            if pc == 0x6AB1 and m[0xE7] == 1:
                st['ticks'] += 1
            if pc == 0x6AEB:
                ev.append(('removed', m[0xE7], st['ticks'], mc.cpu.cycles))
            if pc == 0x79CD and m[0xE7] == 1 and not st['forced']:
                st['forced'] = True
                m[0x60], m[0x61] = 0x8A, 0x6B
                st['score_before'] = (m[0x54], m[0x53], m[0x52])
                ev.append(('forced pickup', st['score_before'], m[0x55]))
            if pc == 0x79FD and st['score_before'] and ev and ev[-1][0] == 'forced pickup':
                ev.append(('after pickup', (m[0x54], m[0x53], m[0x52]), 'E7', m[0xE7], '$5A/5B', m[0x5A] | m[0x5B] << 8))
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
    return ev


if __name__ == '__main__':
    rows = per_level()
    for lv in (1, 2, 3, 5, 7, 9, 12, 13, 16, 21, 26, 27, 30):
        print(lv, rows[lv])
    ev = play(int(sys.argv[1]) if len(sys.argv) > 1 else 1, int(sys.argv[2].replace('_', '')) if len(sys.argv) > 2 else 60_000_000, 3)
    for e in ev:
        print(e)
