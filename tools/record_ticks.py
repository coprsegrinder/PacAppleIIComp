"""TASK-029a: record the complete logical state at EVERY tick of a golden gameplay scenario (development tool; original data from scratch/).
Replays scenario X (A, B, C or D) of tools/golden_trace.py with its stored inputs and pokes (tests/golden/gameplay_traces.json), writes
scratch/ticks_X.bin (LOCAL ONLY: derived from original data, git-ignored) and refuses to write unless the replay reproduces the stored golden
final hash, tick count and checkpoints (so the recording is proven to be the golden run).
File format (little endian):
  'TICK', u32 count (ticks recorded, t = 0 .. count-1), u32 stateSize (1010), 32 bytes golden chain hash after the last recorded tick,
  count * stateSize bytes of state, then u32 nInputs + nInputs * (u32 tick, u32 key = Apple II code: 7-bit ASCII | $80), then u32 nPokes + nPokes * (u32 tick, u32 address, u32 value).
State layout (the golden definition): bytes 0-29 = zero page $52-$6F; 30 = $78; 31 = $79; 32 = $E8; 33 = $E9; 34-113 = $84-$D3 (four ghost records);
114-1009 = maze map $623A-$65B9 (28 columns x 32, column-major).
Keys are pushed after the state of their tick is recorded (before the tick executes); pokes are applied right after the keys of that tick.
Usage: python -I tools/record_ticks.py A      (about 2-4 minutes per scenario; the four can run in parallel)"""
import hashlib, json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G

STATE_SIZE = 1010


def main(name):
    stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][name]
    states = []
    original = G.state_bytes

    def recording(m):
        b = original(m)
        states.append(b)
        return b
    G.state_bytes = recording
    r = G.run(name, stored['inputs'], stored['pokes'])
    for key in ('checkpoints', 'samples', 'events', 'final', 'ticks_done'):
        if r[key] != stored[key]:
            sys.exit('replay differs from the golden file in %s: nothing written' % key)
    h = b''
    final = None
    for t, s in enumerate(states):
        assert len(s) == STATE_SIZE
        h = hashlib.sha256(h + s).digest()
    if h.hex() != stored['final']:
        sys.exit('chain over the recorded states (%s) differs from the golden final hash: nothing written' % h.hex()[:16])
    with open('scratch/ticks_%s.bin' % name, 'wb') as f:
        f.write(b'TICK' + struct.pack('<II', len(states), STATE_SIZE) + h)
        for s in states:
            f.write(s)
        f.write(struct.pack('<I', len(stored['inputs'])))
        for t, k in stored['inputs']:
            f.write(struct.pack('<II', t, (ord(k) if isinstance(k, str) else k) | 0x80))  # the Apple II key code, as push_key sends it
        f.write(struct.pack('<I', len(stored['pokes'])))
        for t, a, v in stored['pokes']:
            f.write(struct.pack('<III', t, a, v))
    print(name, 'ticks recorded', len(states), 'golden ticks_done', stored['ticks_done'], 'chain', h.hex()[:16], 'inputs', len(stored['inputs']), 'pokes', len(stored['pokes']), flush=True)


if __name__ == '__main__':
    main(sys.argv[1])
