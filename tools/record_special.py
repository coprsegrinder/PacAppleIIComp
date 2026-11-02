"""TASK-029d2b: two special recordings that the golden scenarios do not contain (development tool; original data from scratch/).
  tunnel : a game in which Pac-Man is repeatedly PUT (poked) on one of the two tunnel-end junctions (26 at x $5F, y $59 heading left; 29 at x $B9, y $59
           heading right) so that it runs through the tunnel many times; 2,000 ticks, random keys, lives topped up. Writes scratch/ticks_T00.bin
           (format of tools/record_ticks.py; the header hash is the chain hash of the recording itself, recorded after TWO identical replays).
  demo   : the attract-mode demo ($E8 = 1) played by the original's own move stream. Ticks are the executions of $7861 with $E8 <> 0. Writes
           scratch/ticks_DEMO.bin (same format; the inputs and pokes lists are empty) and scratch/demo_stream.bin: for every tick 3 bytes, the stream pointer
           $1B, $1C and the byte at pointer + 1 (the byte the original loads into $63 when Pac-Man acts in that tick, $78D7-$78E5).
  tunnelnat : see natural_tunnel(): the same tunnel runs WITHOUT any poke, steered by keys; written to scratch/ticks_N00.bin.
Usage: python -I tools/record_special.py tunnel|demo|tunnelnat      (about 3-6 minutes)"""
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


def natural_tunnel():
    """tunnelnat: NATURAL play, no pokes at all. A closed-loop controller (breadth-first search over the junction graph read from the program body) presses
    the direction keys so that Pac-Man runs through the left tunnel from junction 26 heading left, then turns at junction 29 and runs through the right
    tunnel, and so on; the ghosts may catch it (restart ticks) and the run ends at game over or after TICKS ticks. Pass 1 plays with the controller and
    records the key presses; pass 2 REPLAYS only the recorded key list through golden_trace.run and must give the identical state chain; the file
    scratch/ticks_N00.bin is written only then (so two independent runs agree). Writes the same format as the other recordings."""
    from apple2 import NeedKey
    from autoplay import start_game
    from play import do_step
    from cov_close import factory
    ticks = 3000
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    rec = lambda j: body[0x760A - 0x4000 + 8 * j: 0x760A - 0x4000 + 8 * j + 8]
    nxt = {j: [rec(j)[2 + d] for d in range(4)] for j in range(64)}
    ok = lambda v: v < 0x80
    KEY = {0: 'Z', 1: 'A', 2: 21, 3: 8}

    def dist_to(goal):
        d = {goal: 0}
        todo = [goal]
        while todo:
            n = todo.pop(0)
            for j in range(64):
                if j not in d and any(ok(v) and v == n for v in nxt[j]):
                    d[j] = d[n] + 1
                    todo.append(j)
        return d
    dists = {26: dist_to(26), 29: dist_to(29)}
    G.SCENARIOS['N00'] = dict(seed=0, ticks=ticks, pokes=[])
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    states, inputs = [], []
    ctl = {'goal': 26, 'last': None, 'x': None, 'wraps': 0, 't': 0, 'done': False}
    step = mc.cpu.step

    def hook():
        m = mc.m
        if mc.cpu.pc == TICK_PC and m[0xE8] == 0 and not ctl['done']:
            t = ctl['t']
            states.append(G.state_bytes(m))
            if t >= ticks:
                ctl['done'] = True
                return step()
            x = m[0x60]
            if ctl['x'] is not None and abs(x - ctl['x']) > 100:
                ctl['wraps'] += 1
                ctl['goal'] = 29 if ctl['goal'] == 26 else 26
            ctl['x'] = x
            goal = ctl['goal']
            exit_dir = 3 if goal == 26 else 2
            cur, nx, d = m[0x64], m[0x65], m[0x62]
            dist = dists[goal]
            want = None
            if ok(nx):
                ahead = 1 + dist.get(nx, 99)
                back = 1 + dist.get(cur, 99)
                if back + 1 < ahead and cur != nx:
                    want = d ^ 1                              # turn round between junctions
                else:
                    node = nx
                    want = exit_dir if node == goal else min((dd for dd in range(4) if ok(nxt[node][dd]) and dist.get(nxt[node][dd], 99) < dist.get(node, 99)), default=d, key=lambda dd: dist[nxt[node][dd]])
            else:
                node = cur
                want = exit_dir if node == goal else min((dd for dd in range(4) if ok(nxt[node][dd]) and dist.get(nxt[node][dd], 99) < dist.get(node, 99)), default=d, key=lambda dd: dist[nxt[node][dd]])
            if want != ctl['last']:
                mc.push_key(KEY[want])
                inputs.append([t, KEY[want]])
                ctl['last'] = want
            ctl['t'] = t + 1
        return step()
    mc.cpu.step = hook
    for _ in range(3000):
        if ctl['done'] or (m_e8(mc) and ctl['t'] > 0):
            break
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    if len(states) < 1000:
        sys.exit('natural tunnel run: only %d ticks (game over?)' % len(states))
    h1 = b''
    for s_ in states:
        h1 = hashlib.sha256(h1 + s_).digest()
    # pass 2: replay of the recorded key list alone
    G.SCENARIOS['N00'] = dict(seed=0, ticks=len(states) - 1, pokes=[])
    states2 = []
    original = G.state_bytes

    def recording(m, states2=states2, original=original):
        b = original(m)
        states2.append(b)
        return b
    G.state_bytes = recording
    try:
        G.run('N00', inputs, [])
    finally:
        G.state_bytes = original
    n = min(len(states), len(states2))
    if states[:n] != states2[:n] or abs(len(states) - len(states2)) > 1:
        sys.exit('the replay of the recorded keys differs from the controlled run: nothing written')
    write('scratch/ticks_N00.bin', states[:n], inputs, [])
    print('natural tunnel ticks', n, 'wraps seen', ctl['wraps'], 'key presses', len(inputs), 'replay identical', flush=True)


def m_e8(mc):
    return mc.m[0xE8] != 0


if __name__ == '__main__':
    {'tunnel': tunnel, 'demo': demo, 'tunnelnat': natural_tunnel}[sys.argv[1]]()
