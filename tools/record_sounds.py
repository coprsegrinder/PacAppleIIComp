"""TASK-032-3: when does the ORIGINAL start each sound, and how long does the game stand still for it? (development tool; original data from scratch/)
Replays a recorded scenario (inputs and pokes as in tools/record_ghost_ram.py) with a hook on the sound entry points
  $7BDC dot, $7BE6 ghost eaten, $7C45 energizer, $7C61 fruit eaten, $7C82 death step ($0A = 6..1), $7C51 bonus life, $7D38 intro jingle, $7CF0 intermission
and writes scratch/sounds_<X>.bin: b'SNDE', u32 count, then per event i32 tick (-1 = before the first tick, e.g. the intro jingle; index of the tick in whose execution it happened = same index as ticks_X.bin states),
u32 code (0 dot, 1 ghost, 2 energizer, 3 fruit, 3 + $0A for the death steps = 4..9, 10 bonus life, 11 jingle, 12 intermission), u32 freeze (cycles from the entry to the return
of the wrapper: the time the game stands still), u32 entry cycle.
Scenarios: A B C D (golden), any letter with scratch/ticks_X.bin (G H L ...), and BL = A's keys with the score poked to 9,980 at tick 50 (bonus life at 10,000).
Usage: python -I tools/record_sounds.py A B C G H BL"""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G

ENTRY = {0x7BDC: 0, 0x7BE6: 1, 0x7C45: 2, 0x7C61: 3, 0x7C82: 4, 0x7C51: 10, 0x7D38: 11, 0x7CF0: 12}


def load(name):
    if name == 'BL':
        a = json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A']
        return a['inputs'], [[50, 0x52, 0x80], [50, 0x53, 0x99], [50, 0x54, 0]], 1500
    if name in 'ABCD' and len(name) == 1:
        s = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][name]
        return s['inputs'], s['pokes'], s.get('ticks_done', G.SCENARIOS[name]['ticks'])
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
    return inputs, pokes, n


def one(name):
    inputs, pokes, n = load(name)
    G.SCENARIOS[name] = dict(seed=0, ticks=n, pokes=[]) if name not in G.SCENARIOS else G.SCENARIOS[name]
    G.SCENARIOS[name]['ticks'] = n
    events, state, pending = [], {'t': -1}, []
    fac = G.factory

    def patched():
        mc = fac()
        inner = mc.cpu.step

        def hook():
            cpu = mc.cpu
            pc = cpu.pc
            if pc == 0x7861 and mc.m[0xE8] == 0:
                state['t'] += 1
            if pc in ENTRY:
                code = ENTRY[pc] + (mc.m[0x0A] - 1 if pc == 0x7C82 else 0) if pc != 0x7C82 else 3 + mc.m[0x0A]
                pending.append([state['t'], code, cpu.sp, cpu.cycles, mc.m[0xE8]])
            r = inner()
            for p in list(pending):
                if cpu.sp > p[2] and len(p) == 5:   # the return address was popped: the wrapper returned
                    p.append(cpu.cycles - p[3])
                    if p[4] == 0:
                        events.append((p[0], p[1], p[5], p[3]))
                    pending.remove(p)
            return r
        mc.cpu.step = hook
        return mc
    G.factory = patched
    try:
        G.run(name, inputs, pokes)
    finally:
        G.factory = fac
    with open('scratch/sounds_%s.bin' % name, 'wb') as f:
        f.write(b'SNDE' + struct.pack('<I', len(events)))
        for e in events:
            f.write(struct.pack('<iIII', *e))
    names = {0: 'dot', 1: 'ghost', 2: 'energizer', 3: 'fruit', 10: 'bonus', 11: 'jingle', 12: 'intermission'}
    cnt = {}
    for e in events:
        cnt[names.get(e[1], 'death%d' % (e[1] - 3))] = cnt.get(names.get(e[1], 'death%d' % (e[1] - 3)), 0) + 1
    print(name, 'ticks', n, 'sound starts', len(events), cnt, flush=True)


if __name__ == '__main__':
    for nm in sys.argv[1:]:
        one(nm)
