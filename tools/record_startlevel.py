"""TASK-034-STARTLEVEL: games that START at a higher level through the original's hidden menu, recorded at every tick (development tool; original data from scratch/).
The recordings tools/record_levels.py makes poke the level counter at tick 5, so they test the tables but not the level-start set-up. Here the real route is used: at "HOW MANY PLAYERS?" Ctrl-V, Ctrl-C,
"R" (anything but C), Return, the rank number, Return ($54E1 = rank), then 1 player and Return to play. The first recorded tick is the game's own first tick at that level; random keys play.
Writes scratch/ticks_X<RR>.bin (same format as tools/record_ticks.py, no pokes) for each requested rank.
Usage: python -I tools/record_startlevel.py 2 5 8 13 21          (about 20-40 seconds per rank)"""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
from apple2 import NeedKey
from play import do_step

TICKS = 700


def start_at(rank):
    def start(mc):
        def run(n=3_000_000):
            try:
                mc.run(n)
            except NeedKey:
                pass
        run()
        mc.push_key(0x20)
        run()                       # Space: the players prompt
        mc.push_key(0x16)           # Ctrl-V
        run(500_000)
        mc.push_key(0x03)           # Ctrl-C: RANK OR CARTOON?
        run()
        for ch in 'R':
            mc.push_key(ord(ch))
        mc.push_key(0x0D)
        run()
        for ch in str(rank):
            mc.push_key(ord(ch))
        mc.push_key(0x0D)
        run()
        mc.push_key(ord('1'))       # one player
        run()
        mc.push_key(0x0D)           # Return: play
        run(1_000_000)
    return start


def one(rank):
    name = 'X%02d' % rank
    G.SCENARIOS[name] = dict(seed=300 + rank, ticks=TICKS, pokes=[])
    inputs = G.make_inputs(300 + rank, TICKS)
    G.start_game = start_at(rank)
    states = []
    original = G.state_bytes

    def recording(m):
        b = original(m)
        states.append(b)
        return b
    G.state_bytes = recording
    try:
        r = G.run(name, inputs, [])
    finally:
        G.state_bytes = original
    if len(states) < 300:
        sys.exit('rank %d: only %d ticks recorded' % (rank, len(states)))
    first = states[0]
    print('rank %d: ticks %d, level at tick 0 = %d, lives %02X, items left %d' % (rank, len(states), first[0x55 - 0x52], first[0x58 - 0x52], first[0x56 - 0x52]), flush=True)
    h = b''
    for s in states:
        h = hashlib.sha256(h + s).digest()
    with open('scratch/ticks_%s.bin' % name, 'wb') as f:
        f.write(b'TICK' + struct.pack('<II', len(states), 1010) + h)
        for s in states:
            f.write(s)
        kept = [(t, k) for t, k in inputs if t < len(states)]
        f.write(struct.pack('<I', len(kept)))
        for t, k in kept:
            if True:
                f.write(struct.pack('<II', t, (ord(k) if isinstance(k, str) else k) | 0x80))
        f.write(struct.pack('<I', 0))


if __name__ == '__main__':
    for a in sys.argv[1:]:
        one(int(a))
