"""TASK-034-FREEZE: a recording in which ghosts are eaten IN A ROW, so that the ghost-eaten freeze ($7DB8) runs while other ghosts are eyes (development tool; original data from scratch/).
No other recording has that: in all of them the ghost eaten is the first of its energizer (the score step $68 is 2) and no ghost is eyes. The freeze's six passes of $805D then walk each
eyes ghost two steps per pass ($806F-$807C), which changes the ghosts' positions and the cost of the tick.
Scenario (letters GH + digit, written to scratch/ticks_GH<n>.bin in the format of tools/record_ticks.py):
  * at tick 100 every 7th plain dot of the map becomes an energizer (as scenario D); random keys (the seed of the scenario); lives topped up every 100 ticks (as T00);
  * from tick 150 whenever a ghost is frightened and not eyes and not in the house, and the waiting time since the last such poke is over, Pac-Man is put on that ghost (x, y and cell counters of its record); when no ghost is frightened the dots just ahead of him become energizers;
    the waiting time cycles through a list (1, 2, 3, 5, 8, 13, 21, 34 ticks) so that the next ghost is eaten while the earlier one is eyes in every phase of its way home.
The first pass decides the pokes from the state of each tick; the pokes are then replayed from the list twice and the two chain hashes must be equal (nothing is written otherwise).
Usage: python -I tools/record_eatchain.py <n> [ticks] [seed]      writes scratch/ticks_GH<n>.bin; then python -I tools/record_ghost_ram.py GH<n> (ghost memory and the .cyc file)"""
import hashlib, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
import cov_close
from record_special import write

n = sys.argv[1]
ticks = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
seed = int(sys.argv[3]) if len(sys.argv) > 3 else 11
name = 'GH' + n
WAITS = [1, 2, 3, 5, 8, 13, 21, 34]
G.SCENARIOS[name] = dict(seed=seed, ticks=ticks, pokes=[])
inputs = G.make_inputs(seed, ticks)


def first_pass():
    pokes, states = [], []
    original = G.state_bytes
    st = {'wait': 0, 'k': 0}

    def decide(m, t):
        mc = cov_close.MACHINES[-1]
        out = []
        if t == 100:
            for a in range(0x623A, 0x65BA):
                if mc.m[a] == 1 and (a - 0x623A) % 7 == 0:
                    out.append((a, 131))
        if t % 100 == 0 and t > 0:
            out.append((0x58, 0x83))
        if st['wait'] > 0:
            st['wait'] -= 1
        elif t >= 150 and mc.m[0x58] != 0 and mc.m[0xE8] == 0:
            for i in range(4):
                g = 0x84 + 20 * i
                if mc.m[g + 8] != 0 and mc.m[g + 9] == 0 and mc.m[g + 17] == 0 and mc.m[g + 13] == 0 and mc.m[g + 14] == 0 and 1 <= mc.m[g + 11] <= 25:   # exactly on a cell centre inside the maze (off-grid pokes lead Pac-Man out of the maze, where the original eats from a copy behind the map: not modelled)
                    out.append((0x60, mc.m[g]))
                    out.append((0x61, mc.m[g + 1]))
                    for k in range(4):                       # his cell counters $6A-$6D as the ghost's ($95-$98: record bytes 11-14), or the original eats from stale cells
                        out.append((0x6A + k, mc.m[g + 11 + k]))
                    st['wait'] = WAITS[st['k'] % len(WAITS)]
                    st['k'] += 1
                    break
            else:
                if not any(mc.m[0x84 + 20 * i + 8] for i in range(4)):   # no ghost frightened: the dots just ahead of Pac-Man become energizers, so that his own next steps eat one
                    col, row, d = mc.m[0x6A], mc.m[0x6B], mc.m[0x62]
                    dc, dr = {0: (0, 1), 1: (0, -1), 2: (1, 0), 3: (-1, 0)}.get(d, (0, 0))
                    for k in (1, 2, 3):
                        c, r = col + dc * k, row + dr * k
                        if 0 <= c < 28 and 0 <= r < 32 and mc.m[0x623A + c * 32 + r] == 1:
                            out.append((0x623A + c * 32 + r, 131))
                    st['wait'] = 10
        for a, v in out:
            mc.m[a] = v
            pokes.append([t, a, v])

    def recording(m):
        b = original(m)
        states.append(b)
        decide(m, len(states) - 1)
        return b
    G.state_bytes = recording
    try:
        G.run(name, inputs, None)
    finally:
        G.state_bytes = original
    return pokes, states


def replay(pokes):
    states = []
    original = G.state_bytes

    def recording(m, states=states):
        b = original(m)
        states.append(b)
        return b
    G.state_bytes = recording
    try:
        G.run(name, inputs, pokes)
    finally:
        G.state_bytes = original
    h = b''
    for s in states:
        h = hashlib.sha256(h + s).digest()
    return h, states


if __name__ == '__main__':
    pokes, s0 = first_pass()
    print('first pass: %d ticks, %d pokes' % (len(s0), len(pokes)), flush=True)
    h1, s1 = replay(pokes)
    h2, s2 = replay(pokes)
    if h1 != h2:
        sys.exit('the two replays differ: nothing written')
    h0 = b''
    for s in s0:
        h0 = hashlib.sha256(h0 + s).digest()
    if h0 != h1:
        sys.exit('the first pass and the replay differ: nothing written')
    # count the eats and those with another ghost already eyes
    eats = multi = 0
    for t in range(1, len(s1)):
        a, b = s1[t - 1], s1[t]
        gh = lambda s: [s[34 + 20 * i:34 + 20 * i + 20] for i in range(4)]
        ga, gb = gh(a), gh(b)
        newly = [i for i in range(4) if ga[i][8] != 0 and ga[i][9] == 0 and gb[i][9] == 1]
        if newly:
            eats += 1
            multi += any(ga[j][9] == 1 for j in range(4) if j not in newly)
    write('scratch/ticks_%s.bin' % name, s1, inputs, pokes)
    print('wrote scratch/ticks_%s.bin: %d ticks, %d pokes, %d ghosts eaten, %d of them with another ghost already eyes, chain %s' % (name, len(s1), len(pokes), eats, multi, h1.hex()[:16]), flush=True)
