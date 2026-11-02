"""TASK-029g4: the drawing calls of every tick, plus oracle pages. Development tool; original data from scratch/.
Replays the first N ticks (default 700) of golden scenario A (stored inputs) with the call collector of tools/capture_hires_vectors.py switched to "record all".
Writes scratch/tick_calls_A.txt (one block per tick: 'tick t' then one line per call: type and parameters; types as in capture_hires_vectors.py: 1 = 3x10 image,
2 = OR blit, 3 = masked clear, 4 = box clear, 5 = small dot, 6 = tile, 7 = 12x3 image, 8 = big sprite, 9 = shifted image) and scratch/pages_A.bin
('PAGS', u32 count, then per page u32 tick + 8,192 bytes: the page at the START of the tick, for every tick that is a multiple of 10).
Also writes scratch/extra_<S>.bin: for EVERY tick 6 bytes at the start of the tick: $E1 $E2 (frightened timer), $E5 $E6 (1UP blink), $E7 (fruit state), $E4.
Usage: python -I tools/record_tick_calls.py [ticks] [scenario letter A-D]   (about 1-2 minutes; files are named with the letter: tick_calls_<S>.txt, pages_<S>.bin)"""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import capture_hires_vectors as H
import golden_trace as G

N = int(sys.argv[1]) if len(sys.argv) > 1 else 700
SC = sys.argv[2] if len(sys.argv) > 2 else 'A'   # golden scenario letter

class Always(set):
    def __contains__(self, k): return False
    def __len__(self): return 0

fac = G.factory
holder = {}
def patched():
    mc = fac()
    col = H.Collector(mc)
    col.seen = {t: Always() for t in range(1, 10)}
    H.LIMIT = 10 ** 9
    inner = col.hook_factory()
    state = {'t': -1}
    pages = []
    extra = holder.setdefault('extra', bytearray())
    def hook():
        if mc.cpu.pc == 0x7861 and mc.m[0xE8] == 0:
            state['t'] += 1
            col.records.append([-1, [state['t']], None, None])
            extra.extend([mc.m[0xE1], mc.m[0xE2], mc.m[0xE5], mc.m[0xE6], mc.m[0xE7], mc.m[0xE4]])
            if state['t'] % 10 == 0:
                pages.append((state['t'], bytes(mc.m[0x2000:0x4000])))
        return inner()
    mc.cpu.step = hook
    holder['col'], holder['pages'] = col, pages
    return mc
G.factory = patched
stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][SC]
G.SCENARIOS[SC]['ticks'] = N
G.run(SC, stored['inputs'], stored['pokes'])
col, pages = holder['col'], holder['pages']
out = []
for t, p, _, _ in col.records:
    if t == -1:
        out.append('tick %d' % p[0])
    else:
        out.append('  type %d  %s' % (t, ' '.join(str(v) for v in p[:5])))
open('scratch/tick_calls_%s.txt' % SC, 'w').write('\n'.join(out))
with open('scratch/pages_%s.bin' % SC, 'wb') as f:
    f.write(b'PAGS' + struct.pack('<I', len(pages)))
    for t, pg in pages:
        f.write(struct.pack('<I', t) + pg)
open('scratch/extra_%s.bin' % SC, 'wb').write(bytes(holder['extra']))
print('ticks', N, 'calls', sum(1 for r in col.records if r[0] != -1), 'pages', len(pages))
