"""TASK-029h3: compare the code that apple2tc found (listing from the merged run data) with our own earlier results.
  A = instruction bytes in apple2tc's assembly listing;  B = our static code set (recursive disassembly from $4027 + dispatch-table targets, tools/recdis.py);
  C = bytes executed in the 30 recordings (run data of tools/a2tc_collect.py cannot give this directly: taken from scratch/cov*.json if present).
Usage: python -I tools/a2tc_compare.py scratch/a2tc_pac.asm   (prints counts and the differing ranges; no original bytes)"""
import os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from recdis import analyse
from classify_gaps import table_targets
from dis6502 import OPS

LO, HI = 0x4000, 0xAA00
A = set()
lines = open(sys.argv[1]).read().split('\n')
for i, l in enumerate(lines):
    m = re.match(r'/\*([0-9A-F]{4})\*/\s{12,}([A-Z]{3})\b', l)
    if m and m.group(2) not in ('DFB', 'DFW', 'ORG', 'EQU'):
        a = int(m.group(1), 16)
        if LO <= a < HI:
            # length from the next listed address
            n = 1
            for l2 in lines[i + 1:i + 6]:
                m2 = re.match(r'/\*([0-9A-F]{4})\*/', l2)
                if m2 and int(m2.group(1), 16) > a:
                    n = int(m2.group(1), 16) - a
                    break
            A.update(range(a, a + n))
body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
code, *_ = analyse(body, LO, [0x4027] + table_targets(body))
B = set()
for a, n in code.items():
    B.update(range(a, a + n))


def ranges(s):
    r, st, p = [], None, None
    for x in sorted(s):
        if st is None:
            st = p = x
        elif x == p + 1:
            p = x
        else:
            r.append((st, p)); st = p = x
    if st is not None:
        r.append((st, p))
    return r


print('apple2tc code bytes', len(A), ' our static code bytes', len(B), ' both', len(A & B))
for name, s in (('only apple2tc', A - B), ('only ours (static)', B - A)):
    r = ranges(s)
    print(name, len(s), 'bytes in', len(r), 'ranges:', ' '.join('$%04X-$%04X' % x for x in r[:40]))
