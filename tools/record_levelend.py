"""TASK-031-1: per-tick recordings ACROSS a level end (development tool; original data from scratch/).
For each requested level L: a game starts, at tick 5 the level $55 is POKED to L, at tick 100 the item counter $56 is poked to 1 (one item left), random keys play
for TICKS ticks, lives topped up every 100 ticks. The level ends naturally when the last item is eaten (cartoon after levels 2, 5, 9 mod 13), the original's own level-end
and level-start code runs, and the recording continues in level L+1. Writes scratch/ticks_E<LL>.bin in the format of tools/record_ticks.py (chain hash of the
recording itself, after two identical replays unless REPLAYS=1). Prints the tick of the level change.
Usage: python -I tools/record_levelend.py 4 5 9 20      (30 s to several minutes per level; cartoons are slow)"""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G

TICKS = 500


def one(level):
    name = 'E%02d' % level
    G.SCENARIOS[name] = dict(seed=300 + level, ticks=TICKS, pokes=[])
    inputs = G.make_inputs(300 + level, TICKS)
    pokes = [[5, 0x55, level], [100, 0x56, 1]] + [[t, 0x58, 0x83] for t in range(100, TICKS, 100)]
    results = []
    for attempt in range(int(os.environ.get('REPLAYS', '2'))):
        states = []
        original = G.state_bytes

        def recording(m, states=states, original=original):
            b = original(m)
            states.append(b)
            return b
        G.state_bytes = recording
        try:
            r = G.run(name, inputs, pokes)
        finally:
            G.state_bytes = original
        h = b''
        for s in states:
            h = hashlib.sha256(h + s).digest()
        results.append((h, states, r))
    if len(results) > 1 and results[0][0] != results[1][0]:
        sys.exit('level %d: two replays differ: nothing written' % level)
    h, states, r = results[0]
    change = [t for t in range(1, len(states)) if states[t][3] != states[t - 1][3] and states[t][3] == level + 1]
    if len(states) < TICKS - 5 or not change:
        sys.exit('level %d: %d ticks, level change at %s: nothing written' % (level, len(states), change))
    with open('scratch/ticks_%s.bin' % name, 'wb') as f:
        f.write(b'TICK' + struct.pack('<II', len(states), 1010) + h)
        for s in states:
            f.write(s)
        f.write(struct.pack('<I', len(inputs)))
        for t, k in inputs:
            f.write(struct.pack('<II', t, (ord(k) if isinstance(k, str) else k) | 0x80))
        f.write(struct.pack('<I', len(pokes)))
        for t, a, v in pokes:
            f.write(struct.pack('<III', t, a, v))
    print(name, 'ticks', len(states), 'level change at tick', change[0], 'chain', h.hex()[:16], flush=True)


if __name__ == '__main__':
    for lv in sys.argv[1:]:
        one(int(lv))
