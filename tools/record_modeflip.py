"""TASK-034-MODEFLIP: a recording in which the ghosts' mode timer runs out in the CHASE phase on the oracle, so that the chase -> corner switch ($808C: reverse the ghosts, corner phase for 140 steps) is seen
(development tool; original data from scratch/). The recordings of the golden games never show it: their chase phases are all ended early by an energizer, because a natural chase lasts 600 steps of Pac-Man.
The game is scenario A of tools/golden_trace.py (same keys) up to the first chase phase; at tick 1500 the countdown $EA/$EB is set to 3 / 0 (poke), and again at tick 3300 (second chase phase).
Writes scratch/ticks_W01.bin (same format as tools/record_ticks.py, the pokes in its trailer); then run  python -I tools/record_ghost_ram.py W01  for the memory beside it.
Usage: python -I tools/record_modeflip.py"""
import hashlib, json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G

TICKS = 4200
POKES = [[1500, 0xEA, 3], [1500, 0xEB, 0], [3300, 0xEA, 3], [3300, 0xEB, 0]]
STATE_SIZE = 1010


def main():
    stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A']
    inputs = [[t, k] for t, k in stored['inputs'] if t < TICKS]
    G.SCENARIOS['W01'] = dict(seed=0, ticks=TICKS, pokes=[])
    states = []
    original = G.state_bytes

    def recording(m):
        b = original(m)
        states.append(b)
        return b
    G.state_bytes = recording
    r = G.run('W01', inputs, POKES)
    G.state_bytes = original
    if r.get('ticks_done', 0) < TICKS:
        sys.exit('the game ended after %s ticks' % r.get('ticks_done'))
    h = b''
    for s in states:
        h = hashlib.sha256(h + s).digest()
    with open('scratch/ticks_W01.bin', 'wb') as f:
        f.write(b'TICK' + struct.pack('<II', len(states), STATE_SIZE) + h)
        for s in states:
            f.write(s)
        f.write(struct.pack('<I', len(inputs)))
        for t, k in inputs:
            f.write(struct.pack('<II', t, (ord(k) if isinstance(k, str) else k) | 0x80))
        f.write(struct.pack('<I', len(POKES)))
        for t, a, v in POKES:
            f.write(struct.pack('<III', t, a, v))
    e9 = [(t, states[t - 1][33], states[t][33]) for t in range(1, len(states)) if states[t][33] != states[t - 1][33]]
    print('ticks', len(states), 'phase changes (tick, from, to):', e9, flush=True)


if __name__ == '__main__':
    main()
