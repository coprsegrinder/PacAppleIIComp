"""TASK-029d2b: two special recordings that the golden scenarios do not contain (development tool; original data from scratch/).
  tunnel : a game in which Pac-Man is repeatedly PUT (poked) on one of the two tunnel-end junctions (26 at x $5F, y $59 heading left; 29 at x $B9, y $59
           heading right) so that it runs through the tunnel many times; 2,000 ticks, random keys, lives topped up. Writes scratch/ticks_T00.bin
           (format of tools/record_ticks.py; the header hash is the chain hash of the recording itself, recorded after TWO identical replays).
  demo   : the attract-mode demo ($E8 = 1) played by the original's own move stream. Ticks are the executions of $7861 with $E8 <> 0. Writes
           scratch/ticks_DEMO.bin (same format; the inputs and pokes lists are empty) and scratch/demo_stream.bin: for every tick 3 bytes, the stream pointer
           $1B, $1C and the byte at pointer + 1 (the byte the original loads into $63 when Pac-Man acts in that tick, $78D7-$78E5).
Usage: python -I tools/record_special.py tunnel|demo      (about 3-6 minutes)"""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G

TICK_PC = 0x7861


def write(path, states, inputs, pokes):
    h = b''
    for s in states:
        h = hashlib.sha256(h + s).digest()
    with open(path, 'wb') as f:
        f.write(b'TICK' + struct.pack('<II', len(states), 1010) + h)
        for s in states:
            f.write(s)
        f.write(struct.pack('<I', len(inputs)))
        for t, k in inputs:
            f.write(struct.pack('<II', t, (ord(k) if isinstance(k, str) else k) | 0x80))
        f.write(struct.pack('<I', len(pokes)))
        for t, a, v in pokes:
            f.write(struct.pack('<III', t, a, v))
    return h


def tunnel():
    ticks = 2000
    G.SCENARIOS['T00'] = dict(seed=77, ticks=ticks, pokes=[])
    inputs = []                      # NO keys: Pac-Man runs straight through the tunnel (random keys reversed it before it reached the edge)
    pokes = [[t, 0x58, 0x83] for t in range(100, ticks, 100)]
    for n, t in enumerate(range(40, ticks - 100, 250)):
        if n % 2 == 0:
            vals = [(0x60, 0x5F), (0x61, 0x59), (0x64, 26), (0x65, 0xFF), (0x62, 3), (0x63, 3)]      # at 26, heading left: through the left tunnel
        else:
            vals = [(0x60, 0xB9), (0x61, 0x59), (0x64, 29), (0x65, 0xFF), (0x62, 2), (0x63, 2)]      # at 29, heading right: through the right tunnel
        pokes += [[t, a, v] for a, v in vals]
    pokes.sort(key=lambda p: p[0])
    hashes = []
    for attempt in range(int(os.environ.get('REPLAYS', '2'))):
        states = []
        original = G.state_bytes

        def recording(m, states=states, original=original):
            b = original(m)
            states.append(b)
            return b
        G.state_bytes = recording
        try:
            G.run('T00', inputs, pokes)
        finally:
            G.state_bytes = original
        h = b''
        for s in states:
            h = hashlib.sha256(h + s).digest()
        hashes.append(h)
    if len(hashes) > 1 and hashes[0] != hashes[1]:
        sys.exit('the two replays differ: nothing written')
    if len(states) < 1500:
        sys.exit('only %d ticks' % len(states))
    write('scratch/ticks_T00.bin', states, inputs, pokes)
    print('tunnel ticks', len(states), 'chain', hashes[0].hex()[:16], 'pokes', len(pokes), flush=True)


def demo():
    from apple2 import NeedKey
    from play import do_step
    from cov_close import factory
    mc = factory()
    for st in ('key:1', 'key:cr'):
        do_step(mc, st, lambda *_: None)
    states, extra = [], bytearray()
    step = mc.cpu.step

    def hook():
        if mc.cpu.pc == TICK_PC and mc.m[0xE8] != 0 and len(states) < 4000:
            states.append(G.state_bytes(mc.m))
            lo, hi = mc.m[0x1B], mc.m[0x1C]
            p = ((hi << 8) | lo) + 1
            extra.extend([lo, hi, mc.m[p & 0xFFFF]])
        return step()
    mc.cpu.step = hook
    for _ in range(4000):
        if len(states) >= 4000:
            break
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)           # never reached in practice: the demo needs no key; guards against a prompt
    if len(states) < 1000:
        sys.exit('only %d demo ticks' % len(states))
    h = write('scratch/ticks_DEMO.bin', states, [], [])
    open('scratch/demo_stream.bin', 'wb').write(bytes(extra))
    print('demo ticks', len(states), 'chain', h.hex()[:16], flush=True)


if __name__ == '__main__':
    {'tunnel': tunnel, 'demo': demo}[sys.argv[1]]()
