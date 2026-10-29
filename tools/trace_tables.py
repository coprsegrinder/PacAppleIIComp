"""Find the tables behind the self-modified table readers (development tool; uses original data from scratch/).
For each reader instruction (abs,X / abs,Y with a patched operand) records, per base address value, the offsets read and how often.
Usage: python -I tools/trace_tables.py [max_instr]     -> prints a summary, writes scratch/tables.json
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey

# instruction start addresses whose absolute operand is rewritten at runtime (from docs/DYNAMIC_COVERAGE.md / xrefs)
READERS = {0x55B1: 'blit LDA tbl,X', 0x6A4A: 'LDA tbl,X (6A4B patched)', 0x6950: 'LDA tbl,X', 0x6959: 'LDA tbl,X',
           0x69B7: 'LDA tbl,X (69B8 patched)', 0x6EB7: 'LDA tbl,X (6EB8 patched)', 0x6EFF: 'ORA tbl,X (6F00 patched)'}


def install_tracker(mc, readers):
    """Hook mc.read: for instructions in `readers` (abs,X / abs,Y), record (pc, base) -> [count, min_off, max_off].
    Needs mc.enable_coverage() to have run (it supplies mc.cur_pc)."""
    stats = {}
    plain = mc.read

    def read(a):
        pc = mc.cur_pc
        if pc in readers:
            base = mc.m[pc + 1] | mc.m[pc + 2] << 8
            off = a - base
            if 0 <= off < 256:
                s = stats.setdefault((pc, base), [0, off, off])
                s[0] += 1
                s[1] = min(s[1], off)
                s[2] = max(s[2], off)
        return plain(a)
    mc.read = read
    return stats


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 80_000_000
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    mc.run(10_000_000, stop_pc=0x4027)
    mc.enable_coverage()
    stats = install_tracker(mc, READERS)
    mc.push_key('1')
    try:
        mc.run(10_000_000)
    except NeedKey:
        pass
    mc.push_key('N')
    mc.run(limit)
    rows = sorted((pc, base, c, lo, hi) for (pc, base), (c, lo, hi) in stats.items())
    json.dump(rows, open('scratch/tables.json', 'w'))
    for pc, base, c, lo, hi in rows:
        print('%04X base=%04X reads=%d offsets %d..%d (extent %04X-%04X)' % (pc, base, c, lo, hi, base + lo, base + hi))
    print(len(rows), 'distinct (reader, base) pairs')


if __name__ == '__main__':
    main()
