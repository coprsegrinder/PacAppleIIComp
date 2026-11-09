"""TASK-034-NEARGHOST: games at level 5 and up in which a ghost is repeatedly put NEAR Pac-Man on the oracle, to see the near-ghost delay of the contact test ($8B34-$8B65: from level 5 on, a ghost in
his row or column 6-9 pixels away may add one tick to his step countdown $59) fire (development tool; original data from scratch/).
The recordings of the golden games and of tools/record_levels.py hold only one tick where it fires. Here the game is started at the rank R through the hidden menu (tools/record_startlevel.py) and every
12 ticks a ghost (in turn) is moved to the row or column of Pac-Man, 6-9 pixels away, moving horizontally or vertically; the lives are topped up every 100 ticks. The ghost bytes written are in the pokes of
the recording (the replay test takes the ghosts from the recording, so only Pac-Man's side is tested). Random keys play.
Writes scratch/ticks_Y<RR>.bin (format of tools/record_ticks.py).
Usage: python -I tools/record_nearghost.py 5 9 13 21          (about a minute per rank)"""
import hashlib, os, random, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
from apple2 import NeedKey
from cov_close import factory
from record_startlevel import start_at

TICKS = 1500
EVERY = 6


def one(rank):
    name = 'Y%02d' % rank
    rng = random.Random(500 + rank)
    inputs = G.make_inputs(500 + rank, TICKS)
    by_tick = {}
    for t, k in inputs:
        by_tick.setdefault(t, []).append(k)
    mc = factory()
    from play import do_step
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_at(rank)(mc)
    states, pokes = [], []
    step = mc.cpu.step
    done = {'v': False}

    def poke(t, a, v):
        mc.m[a] = v & 0xFF
        pokes.append([t, a, v & 0xFF])

    def hook():
        if mc.cpu.pc == G.TICK_PC and mc.m[0xE8] == 0 and not done['v']:
            t = len(states)
            states.append(G.state_bytes(mc.m))
            if t >= TICKS:
                done['v'] = True
            else:
                for k in by_tick.get(t, []):
                    mc.push_key(k)
                if t % 10 == 5:
                    poke(t, 0x58, 0x83)
                if t >= 20 and t % EVERY == 0:
                    g = (t // EVERY) % 4
                    base = 0x84 + 20 * g
                    px, py = mc.m[0x60], mc.m[0x61]
                    d = rng.randint(6, 9) * rng.choice([-1, 1])
                    if rng.random() < 0.5:
                        gx, gy, gd = (px + d) & 0xFF, py, (3 if d > 0 else 2)    # same row, moving towards Pac-Man horizontally
                    else:
                        gx, gy, gd = px, (py + d) & 0xFF, (1 if d > 0 else 0)    # same column, vertically
                    poke(t, base, gx)
                    poke(t, base + 1, gy)
                    poke(t, base + 2, gd)
        return step()
    mc.cpu.step = hook
    guard = 0
    while not done['v'] and guard < 4000:
        guard += 1
        if mc.m[0xE8] != 0 and states:
            break
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    n = len(states)
    h = b''
    for s in states:
        h = hashlib.sha256(h + s).digest()
    kept = [(t, k) for t, k in inputs if t < n]
    with open('scratch/ticks_%s.bin' % name, 'wb') as f:
        f.write(b'TICK' + struct.pack('<II', n, 1010) + h)
        for s in states:
            f.write(s)
        f.write(struct.pack('<I', len(kept)))
        for t, k in kept:
            f.write(struct.pack('<II', t, (ord(k) if isinstance(k, str) else k) | 0x80))
        f.write(struct.pack('<I', len(pokes)))
        for t, a, v in pokes:
            f.write(struct.pack('<III', t, a, v))
    plus = sum(1 for i in range(1, n) if (states[i][0x59 - 0x52] - states[i - 1][0x59 - 0x52]) & 0xFF == 1)
    print('rank %d: %d ticks, %d pokes, level at the end %d, game over: %s' % (rank, n, len(pokes), states[-1][0x55 - 0x52], mc.m[0xE8] != 0), flush=True)


if __name__ == '__main__':
    for a in sys.argv[1:]:
        one(int(a))
