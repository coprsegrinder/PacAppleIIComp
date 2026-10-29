"""Which instructions read the program's non-code bytes at runtime? (development tool; uses original data from scratch/)
Runs the same scenario as trace_coverage.py (keys 1,N then free run) and reports, per reading instruction, how many distinct
non-code addresses in $4000-$A9FF it read. Writes scratch/readers.json. Usage: python -I tools/trace_readers.py [max_instr]
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey
from recdis import analyse
from static_plus import table_targets


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 80_000_000
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    code, *_ = analyse(body, 0x4000, [0x4027] + table_targets(body))
    cb = set()
    for a, n in code.items():
        cb.update(range(a, a + n))
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    mc.run(10_000_000, stop_pc=0x4027)
    mc.enable_coverage()                      # supplies mc.cur_pc
    readers = {}
    plain_read = mc.read

    def read(a):
        if 0x4000 <= a < 0xAA00 and a not in cb:
            readers.setdefault(mc.cur_pc, set()).add(a)
        return plain_read(a)
    mc.read = read
    mc.push_key('1')
    try:
        mc.run(10_000_000)
    except NeedKey:
        pass
    mc.push_key('N')
    mc.run(limit)
    rows = sorted(((pc, len(s), min(s), max(s)) for pc, s in readers.items()), key=lambda r: -r[1])
    json.dump(rows, open('scratch/readers.json', 'w'))
    for pc, n, lo, hi in rows[:25]:
        print('%04X distinct=%d range %04X-%04X' % (pc, n, lo, hi))
    print(len(rows), 'reading instructions;', len(set().union(*readers.values())), 'distinct non-code bytes read')


if __name__ == '__main__':
    main()
