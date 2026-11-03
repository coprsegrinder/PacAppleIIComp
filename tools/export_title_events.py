"""TASK-033-4b: writes scratch/title_events.bin for the C++ test (development tool; original data from scratch/): the oracle's ordered events of one title turn from T0 on.
Format: 'TEV1', u32 count, then per event 7 x i32: time (cycles after T0), kind, p0..p4.  Kinds: 1 clear, 2 hline (x0 low byte, x1, y, $1D, $1E), 3 blit (data address, column, row, width, rows left),
4 poll, 5 delay (counter), 6 dispatch (image address, x, y, $1D, $1E), 7 dot (x, y), 8 tile (table, x, y).   Kinds 9 (masked clear: table, x, y) and 10 (shifted image: pointer, x, y, $1D, $1E) are the chase's. Usage: python -I tools/export_title_events.py [title|chase]"""
import json, struct, sys
NAME = sys.argv[1] if len(sys.argv) > 1 else 'title'
D = json.load(open('scratch/events_%s.json' % NAME))
KIND = {'clear': 1, 'hline': 2, 'blit': 3, 'poll': 4, 'delay': 5, 'dispatch': 6, 'dot': 7, 'tile': 8, 'mclear': 9, 'shifted': 10}
rows = []
first_clear_seen = False
for e in D['events']:
    k = KIND.get(e[1])
    if not k:
        continue
    p = list(e[2:])
    if e[1] == 'hline':
        p[0] &= 255
    if e[1] == 'delay':
        p = [p[0]]
    p = (p + [0] * 5)[:5]
    rows.append([e[0], k] + p)
with open('scratch/%s_events.bin' % NAME, 'wb') as f:
    f.write(b'TEV1' + struct.pack('<I', len(rows)))
    for r in rows:
        f.write(struct.pack('<7i', *r))
print(len(rows), 'events written; T0 was', D['t0'])
