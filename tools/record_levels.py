"""TASK-029d1b: per-tick recordings at HIGHER LEVELS (the golden scenarios only reach level 3). Development tool; original data from scratch/.
For each requested level L, a game is started and at tick 5 the level counter $55 is POKED to L (so the level-dependent tables are read as in level L);
random keys play for TICKS ticks and the lives counter is topped up to $83 every 100 ticks so that the run cannot end in game over.
This is an EXPERIMENT SCENARIO, not a natural level sequence: only code that reads $55 live (the tables) is tested this way, not the level-start setup.
Writes scratch/ticks_L<LL>.bin in the format of tools/record_ticks.py; the 32-byte header hash is the chain hash of the recording itself (there is no
golden file for these runs), recorded after replaying the run TWICE and requiring identical chain hashes (set REPLAYS=1 to skip the second replay).
Usage: python -I tools/record_levels.py 4 5 6 ...      (about 15-30 seconds per level)"""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G

TICKS = 700


def one(level):
    name = 'L%02d' % level
    G.SCENARIOS[name] = dict(seed=100 + level, ticks=TICKS, pokes=[])
    inputs = G.make_inputs(100 + level, TICKS)
    pokes = [[5, 0x55, level]] + [[t, 0x58, 0x83] for t in range(100, TICKS, 100)]
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
    if len(states) < 600:
        sys.exit('level %d: only %d ticks (game over?)' % (level, len(states)))
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
    print(name, 'ticks', len(states), 'events', r['events'], 'chain', h.hex()[:16], flush=True)


if __name__ == '__main__':
    for lv in sys.argv[1:]:
        one(int(lv))
