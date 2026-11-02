"""TASK-030e1: the memory the ghost logic reads and writes OUTSIDE the logical state of the tick recordings, recorded per tick (development tool; original data from scratch/).
For each tick (the same ticks as scratch/ticks_X.bin: one record per execution of $7861 with $E8 = 0, taken BEFORE the tick's keys and pokes) it stores the whole zero page
$0000-$00FF and the four ghost move-queue pages $0800-$0BFF (ghost 1..4 queues at $0800, $0900, $0A00, $0B00: found from the queue pointers $75/$76 of the ghost records).
File scratch/ghostram_X.bin: b'GRAM', uint32 ticks, uint32 bytes per tick (1280), then the records.
The keys and pokes are those stored in tests/golden/gameplay_traces.json (A-D) or in the trailer of scratch/ticks_X.bin (every other recording); two-player letters use key 2.
Usage: python -I tools/record_ghost_ram.py <X>      (about 2-4 minutes; the replay must reproduce the recording's chain hash, which is checked)"""
import hashlib, json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
import cov_close

name = sys.argv[1]
if len(name) == 1 and name in 'ABCD':
    stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][name]
    inputs, pokes = stored['inputs'], stored['pokes']
    n = stored.get('ticks_done', G.SCENARIOS[name]['ticks'])
    G.SCENARIOS[name]['ticks'] = n
else:
    d = open('scratch/ticks_%s.bin' % name, 'rb').read()
    n = struct.unpack('<I', d[4:8])[0] - 1
    pos = 44 + (n + 1) * 1010
    k = struct.unpack('<I', d[pos:pos + 4])[0]
    pos += 4
    inputs = [[struct.unpack('<II', d[pos + 8 * i:pos + 8 * i + 8])[0], struct.unpack('<II', d[pos + 8 * i:pos + 8 * i + 8])[1] & 0x7F] for i in range(k)]
    pos += 8 * k
    k = struct.unpack('<I', d[pos:pos + 4])[0]
    pos += 4
    pokes = [list(struct.unpack('<III', d[pos + 12 * i:pos + 12 * i + 12])) for i in range(k)]
    G.SCENARIOS[name] = dict(seed=0, ticks=n, pokes=[])
    if name in ('P', 'Q', 'R', 'S', 'U'):
        from play import do_step

        def two_players(mc):
            for st in ('key:sp', '3_000_000', 'key:2', '3_000_000', 'key:cr', '1_000_000'):
                do_step(mc, st, lambda *_: None)
        G.start_game = two_players
recs, states = [], []
orig = G.state_bytes


def rec(m):
    b = orig(m)
    states.append(b)
    recs.append(bytes(m[0:256]) + bytes(m[0x800:0xC00]))
    return b


G.state_bytes = rec
G.run(name, inputs, pokes)
G.state_bytes = orig
h = b''
for s in states:
    h = hashlib.sha256(h + s).digest()
if name not in ('DEMO',):
    d = open('scratch/ticks_%s.bin' % name, 'rb').read() if os.path.exists('scratch/ticks_%s.bin' % name) else None
    if d is not None:
        ok = d[12:44] == h or name in 'ABCD'
        print('chain hash of the replay equals the recording:', d[12:44] == h, '(A-D: golden file has its own header hash; compared by states below)')
        m = min(len(states), struct.unpack('<I', d[4:8])[0])
        same = all(d[44 + t * 1010:44 + (t + 1) * 1010] == states[t] for t in range(m))
        print('all', m, 'states equal the recording:', same)
with open('scratch/ghostram_%s.bin' % name, 'wb') as f:
    f.write(b'GRAM' + struct.pack('<II', len(recs), 1280))
    for r in recs:
        f.write(r)
print(name, 'ticks', len(recs))
