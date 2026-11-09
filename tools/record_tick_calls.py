"""TASK-029g4: the drawing calls of every tick, plus oracle pages. Development tool; original data from scratch/.
Replays the first N ticks (default 700) of golden scenario A (stored inputs) with the call collector of tools/capture_hires_vectors.py switched to "record all".
Writes scratch/tick_calls_A.txt (one block per tick: 'tick t' then one line per call: type and parameters; types as in capture_hires_vectors.py: 1 = 3x10 image,
2 = OR blit, 3 = masked clear, 4 = box clear, 5 = small dot, 6 = tile, 7 = 12x3 image, 8 = big sprite, 9 = shifted image) and scratch/pages_A.bin
('PAGS', u32 count, then per page u32 tick + 8,192 bytes: the page at the START of the tick, for every tick that is a multiple of 10).
Also writes scratch/extra_<S>.bin: for EVERY tick 6 bytes at the start of the tick: $E1 $E2 (frightened timer), $E5 $E6 (1UP blink), $E7 (fruit state), $E4.
Usage: python -I tools/record_tick_calls.py [ticks] [scenario letter A-D]   (about 1-2 minutes; files are named with the letter: tick_calls_<S>.txt, pages_<S>.bin)"""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import capture_hires_vectors as H
import golden_trace as G

N = int(sys.argv[1]) if len(sys.argv) > 1 else 700
SC = sys.argv[2] if len(sys.argv) > 2 else 'A'   # golden scenario letter

class Always(set):
    def __contains__(self, k): return False
    def __len__(self): return 0

fac = G.factory
holder = {}
def patched():
    mc = fac()
    col = H.Collector(mc)
    col.seen = {t: Always() for t in range(1, 10)}
    H.LIMIT = 10 ** 9
    inner = col.hook_factory()
    state = {'t': -1}
    pages = []
    extra = holder.setdefault('extra', bytearray())
    def hook():
        if mc.cpu.pc == 0x7861 and mc.m[0xE8] == 0:
            state['t'] += 1
            if SC in ('T3', 'T4', 'T5'):   # every player is left with ONE life 5 ticks into his turn, so that each of them dies at his first catch (TASK-034-SWITCH)
                dyn = holder.setdefault('dyn', {'cur': 0, 'turn': 0, 'done': False, 'pokes': []})
                cur = mc.m[0x50]
                if cur != dyn['cur']:
                    dyn.update(cur=cur, turn=state['t'], done=False, placed=False)
                if not dyn['done'] and state['t'] >= dyn['turn'] + 5:   # (every player is left with TWO lives in his first turn, so that the rotation comes round again; the second death ends his game)
                    if (mc.m[0x58] & 0x7F) > 2:
                        mc.m[0x58] = 0x82
                        dyn['pokes'].append([state['t'], 0x58, 0x82])
                    dyn['done'] = True
                if dyn['done'] and not dyn.get('placed') and state['t'] >= dyn['turn'] + 40:   # then Pac-Man is put on a ghost that is out of the house and not blue: the next contact test catches him
                    for gb in (0x84, 0x98, 0xAC, 0xC0):
                        if mc.m[gb + 17] == 0 and mc.m[gb + 9] == 0 and mc.m[gb + 8] == 0:
                            for addr, val in ((0x60, mc.m[gb]), (0x61, mc.m[gb + 1])):
                                mc.m[addr] = val
                                dyn['pokes'].append([state['t'], addr, val])
                            dyn['placed'] = True
                            break
            col.records.append([-1, [state['t']], None, None])
            extra.extend([mc.m[0xE1], mc.m[0xE2], mc.m[0xE5], mc.m[0xE6], mc.m[0xE7], mc.m[0xE4]])
            holder.setdefault('extra2', bytearray()).extend([mc.m[0x50], mc.m[0x51]])
            if state['t'] % 10 == 0:
                pages.append((state['t'], bytes(mc.m[0x2000:0x4000])))
        return inner()
    mc.cpu.step = hook
    holder['col'], holder['pages'] = col, pages
    return mc
G.factory = patched
import hashlib
CUSTOM = {
    # own scenarios (not golden): A's keys plus these pokes [tick, address, value]
    'G': [[50, 0x57, 3], [300, 0x60, 0x8B], [300, 0x61, 0x6B]],     # fruit after 3 items; at tick 300 Pac-Man is put on the fruit's place (x $8B, y $6B)
    'H': [[5, 0x55, 6], [100, 0x56, 1]],
    'K': [[200, 0x84, 0x8B], [200, 0x85, 0x47], [200, 0x95, 1], [200, 0xA0, 1], [200, 0xB4, 1], [200, 0xC8, 1], [200, 0xE1, 0xC0], [200, 0xE2, 0], [200, 0xE4, 1]],   # as J, and ghosts 2-4 frightened (blue image, flag $1E = 1) so that the last draw before the door redraw leaves $1E = 1
    'L': [[5, 0x55, 14], [100, 0x56, 1]],       # level 14 -> 15
    'M': [[5, 0x55, 25], [100, 0x56, 1]],       # level 25 -> 26 (the last table entry)
    'Q': [[1990, 0x58, 0x81]],                  # two players as P; at tick 1990 (player 2 is playing) player 2 is left with one life: its death ends its game
    'R': [[1990, 0x58, 0x81], [4230, 0x58, 0x81]],   # as Q, and at tick 4230 (player 1 is playing again) player 1 is left with one life too: both games end
    'S': [[5, 0x58, 0x81], [1990, 0x58, 0x81]],   # two players: player 1 has one life from the start (its game ends first), player 2 is left with one life at tick 1990
    'U': [[2000, 0x56, 1]],                      # two players as P; at tick 2000 (player 2 is playing) one item is left: player 2's level ends
    'P': [],                                    # TWO PLAYERS: A's keys, the players prompt answered with 2
    'J': [[200, 0x84, 0x8B], [200, 0x85, 0x47], [200, 0x95, 1]],      # ghost 1 is put on the door cell with its in-house flag set: it runs the house-exit script and redraws the door with $1E = its own flag 1                            # level 6; at tick 100 one item is left: the next item ends the level (-> level 7: icons)
}
for _l in range(2, 27):   # Z02-Z26 (TASK-034-ICONS): the level before is poked at tick 5, its last item at tick 100: the level end draws the screen of level _l with its icon
    CUSTOM['Z%02d' % _l] = [[5, 0x55, _l - 1], [110 if _l in (12, 13) else 100, 0x56, 1]]   # (for 12 and 13 at tick 100 Pac-Man eats an item in the same tick as the poke, which the level-end detection of the tests cannot see)
if SC in ('T3', 'T4', 'T5'):
    stored = dict(json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A'])
    G.SCENARIOS[SC] = dict(G.SCENARIOS['A'])
elif SC in CUSTOM:
    stored = dict(json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A'])
    stored['pokes'] = CUSTOM[SC]
    G.SCENARIOS[SC] = dict(G.SCENARIOS['A'])
elif SC == 'F':
    # scenario F (own, not golden): the keys of A; at tick 100 all four ghosts are made frightened and the frightened timer $E1/$E2 is set to $00B0, so the timer
    # runs below $80 and the flashing frames appear (no energizer is eaten in A's first 700 ticks). Also writes scratch/ticks_F.bin like tools/record_ticks.py.
    stored = dict(json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A'])
    stored['pokes'] = [[50, 0x57, 3], [400, 0x8C, 1], [400, 0xA0, 1], [400, 0xB4, 1], [400, 0xC8, 1], [400, 0xE1, 0x70], [400, 0xE2, 0], [400, 0xE4, 1]]   # fruit after 3 items; frightened ghosts with the timer at $B0 at tick 400 (the ghosts are out of the house by then)
    G.SCENARIOS['F'] = dict(G.SCENARIOS['A'])
else:
    stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][SC]
G.SCENARIOS[SC]['ticks'] = N
if SC in ('P', 'Q', 'R', 'S', 'U', 'T3', 'T4', 'T5'):
    def two_players(mc):
        from play import do_step
        for st in ('key:sp', '3_000_000', 'key:%s' % (SC[1] if SC[0] == 'T' else '2'), '3_000_000', 'key:cr', '1_000_000'):
            do_step(mc, st, lambda *_: None)
    G.start_game = two_players
states = []
orig_state = G.state_bytes
def rec(m):
    b = orig_state(m)
    states.append(b)
    return b
G.state_bytes = rec
G.run(SC, stored['inputs'], stored['pokes'])
G.state_bytes = orig_state
if SC in ('T3', 'T4', 'T5'):
    stored['pokes'] = holder.get('dyn', {}).get('pokes', [])
if SC in ('F', 'G', 'H', 'J', 'K', 'L', 'M', 'P', 'Q', 'R', 'S', 'U') or SC.startswith('Z') or SC in ('T3', 'T4', 'T5'):
    import struct as _st
    states = states[:N + 1]   # a run that ends early (game over leaves the main loop) has fewer states than requested
    h = b''
    for x in states[:N + 1]:
        h = hashlib.sha256(h + x).digest()
    with open('scratch/ticks_%s.bin' % SC, 'wb') as f:
        f.write(b'TICK' + _st.pack('<II', len(states), 1010) + h)
        for x in states[:N + 1]:
            f.write(x)
        f.write(_st.pack('<I', len(stored['inputs'])))
        for t, k in stored['inputs']:
            f.write(_st.pack('<II', t, (ord(k) if isinstance(k, str) else k) | 0x80))
        f.write(_st.pack('<I', len(stored['pokes'])))
        for t, a, v in stored['pokes']:
            f.write(_st.pack('<III', t, a, v))
col, pages = holder['col'], holder['pages']
out = []
for t, p, _, _ in col.records:
    if t == -1:
        out.append('tick %d' % p[0])
    else:
        out.append('  type %d  %s' % (t, ' '.join(str(v) for v in p[:5])))
open('scratch/tick_calls_%s.txt' % SC, 'w').write('\n'.join(out))
with open('scratch/pages_%s.bin' % SC, 'wb') as f:
    f.write(b'PAGS' + struct.pack('<I', len(pages)))
    for t, pg in pages:
        f.write(struct.pack('<I', t) + pg)
open('scratch/extra_%s.bin' % SC, 'wb').write(bytes(holder['extra']))
open('scratch/extra2_%s.bin' % SC, 'wb').write(bytes(holder['extra2']))   # $50 (current player) and $51 (number of players) per tick
print('ticks', N, 'calls', sum(1 for r in col.records if r[0] != -1), 'pages', len(pages))
