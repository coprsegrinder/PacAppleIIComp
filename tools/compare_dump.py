"""Compare the headless machine with an AppleWin memory dump taken at the 'HOW MANY PLAYERS?' prompt (development tool; original data from scratch/).
Usage: python -I tools/compare_dump.py <dump.bin> <instructions_before_space> [more instruction counts ...]
Our key sequence mirrors the AppleWin session: '1', Enter at the menus, run N instructions of attract, then Space (+Enter), then wait at the prompt.
For each N it prints how many bytes differ per region and, inside the game area, how many differ in code vs data (static code map).
Writes scratch/dump_diff.txt for the last N (runs of differing addresses).
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine
from recdis import analyse
from static_plus import table_targets, bytes_of

REGIONS = [('zero page', 0x0000, 0x0100), ('stack', 0x0100, 0x0200), ('$200-$3FF', 0x0200, 0x0400), ('text page 1', 0x0400, 0x0800),
           ('text page 2', 0x0800, 0x0C00), ('$C00-$1FFF', 0x0C00, 0x2000), ('hires 1', 0x2000, 0x4000), ('game $4000-$A9FF', 0x4000, 0xAA00),
           ('$AA00-$BFFF', 0xAA00, 0xC000)]


def to_runs(addrs):
    out = []
    for x in sorted(addrs):
        if out and x == out[-1][1] + 1:
            out[-1][1] = x
        else:
            out.append([x, x])
    return out


def our_state(n_attract):
    mc = make_machine()
    mc.push_key('1')
    mc.push_key(0x0D)
    try:
        mc.run(n_attract)
    except NeedKey:
        pass
    mc.push_key(' ')
    mc.push_key(0x0D)
    try:
        mc.run(20_000_000)
    except NeedKey:
        pass
    return mc


def main():
    dump = open(sys.argv[1], 'rb').read()
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    code, *_ = analyse(body, 0x4000, [0x4027] + table_targets(body))
    static = bytes_of(code)
    last = None
    for n in [int(a.replace('_', '')) for a in sys.argv[2:]]:
        mc = our_state(n)
        at_prompt = mc.cpu.pc == 0xFD0C
        diff = {a for a in range(0xC000) if mc.m[a] != dump[a]}
        print('--- %d attract instructions, parked in RDKEY: %s, cycles %d' % (n, at_prompt, mc.cpu.cycles))
        for name, lo, hi in REGIONS:
            d = [a for a in diff if lo <= a < hi]
            extra = ''
            if name.startswith('game'):
                dc = [a for a in d if a in static]
                extra = ' | in static code: %d, in data: %d' % (len(dc), len(d) - len(dc))
            print('  %-18s %5d differing of %5d%s' % (name, len(d), hi - lo, extra))
        last = (mc, diff, static)
    mc, diff, static = last
    with open('scratch/dump_diff.txt', 'w') as f:
        for a, b in to_runs(a for a in diff if 0x4000 <= a < 0xAA00):
            f.write('%04X-%04X n=%d code=%s ours=%s dump=%s\n' % (a, b, b - a + 1, any(x in static for x in range(a, b + 1)),
                                                                 bytes(mc.m[a:min(b, a + 7) + 1]).hex(), 'see dump'))
    print('detail: scratch/dump_diff.txt')


if __name__ == '__main__':
    main()
