"""TASK-031-1: inventory of the level changes in the per-tick recordings (development tool; reads scratch/ticks_*.bin, local only).
For each recording prints the ticks where the level $55 (state byte 3) changes, and where the item counter $56 (byte 4) first reaches 0 / jumps back up.
Usage: python -I tools/level_changes.py [names...]   (default: all scratch/ticks_*.bin except the poke-to-level L<nn> runs)"""
import glob, os, re, struct, sys

def scan(path):
    d = open(path, 'rb').read()
    n, sz = struct.unpack('<II', d[4:12])
    out, prev = [], None
    for t in range(n):
        s = d[44 + t * sz:44 + (t + 1) * sz]
        lv, items = s[3], s[4]
        if prev is not None and lv != prev[0]:
            out.append('t=%d level %d->%d (items %d->%d)' % (t, prev[0], lv, prev[1], items))
        elif prev is not None and prev[1] == 0 and items != 0 and lv == prev[0]:
            out.append('t=%d items 0->%d same level %d' % (t, items, lv))
        prev = (lv, items)
    return n, out

names = sys.argv[1:] or [re.match(r'ticks_(.*)\.bin', os.path.basename(p)).group(1) for p in sorted(glob.glob('scratch/ticks_*.bin'))]
for nm in names:
    if re.fullmatch(r'L\d\d', nm) and len(sys.argv) == 1:
        continue
    n, out = scan('scratch/ticks_%s.bin' % nm)
    print(nm, n, 'ticks;', '; '.join(out) if out else 'no level change')
