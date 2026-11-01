"""TASK-034a: golden gameplay traces from the oracle (development tool; original data from scratch/; only hashes and small samples are stored).
A TICK is one pass of the game's main loop = one execution of $7861 (Pac-Man's step timer) with the demo flag $E8 = 0. The port has no cycle
timing, so inputs are keyed by tick: before tick t (after recording the state) the listed keys are pressed and the listed pokes are applied.
LOGICAL STATE (what the port must reproduce, in this order; everything else in memory is scratch or code):
  zero page $52-$6F (score BCD, level, dot counters, lives, fruit timers, Pac-Man position/direction/junctions, ghost score step, ...),
  $78 $79 $E8 $E9 (ghost mode flags, demo flag), ghost records $84-$D3 (80 bytes), maze dot map $623A-$65B9 (896 bytes).
A chain hash h(t) = SHA-256(h(t-1) + state(t)) is checkpointed every 500 ticks; samples of readable fields every 50 ticks help to find where a
port diverges. Scenarios: A (random keys), B (random keys + level-end poke), C (two level ends; passes the level-2 cartoon), D (every 7th dot of the map made an energizer at tick 100; later Pac-Man is put on a frightened ghost up to 4 times, so ghosts get eaten).
Usage: python -I tools/golden_trace.py [out.json]   (writes tests/golden/gameplay_traces.json by default; about 3 minutes)
       python -I tools/golden_trace.py check          (replays the stored inputs and compares; used by tests/test_golden_gameplay.py)"""
import hashlib, json, os, random, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory

TICK_PC = 0x7861
MARKS = {0x7AC7: 'energizer_eaten', 0x7AF4: 'dot_eaten', 0x8B9D: 'collision_resolved', 0x8BDD: 'pacman_caught', 0x6CF4: 'level_complete'}
SCENARIOS = {
    'A': dict(seed=7, ticks=12000, pokes=[]),
    'B': dict(seed=21, ticks=12000, pokes=[('level_end', 1, 300)]),
    'C': dict(seed=33, ticks=14000, pokes=[('level_end', 1, 300), ('level_end', 2, 300)]),
    'D': dict(seed=5, ticks=16000, pokes=[('energizers', 0, 100), ('eat_ghost', 0, 4)]),
}
FIELDS = [('score_lo', 0x52), ('score_mid', 0x53), ('score_hi', 0x54), ('level', 0x55), ('dots_left', 0x56), ('lives', 0x58),
          ('pac_x', 0x60), ('pac_y', 0x61), ('pac_dir', 0x62), ('mode78', 0x78), ('mode79', 0x79), ('modeE9', 0xE9),
          ('g1_x', 0x84), ('g1_y', 0x85), ('g2_x', 0x98), ('g2_y', 0x99), ('g3_x', 0xAC), ('g3_y', 0xAD), ('g4_x', 0xC0), ('g4_y', 0xC1)]


def state_bytes(m):
    return bytes(m[0x52:0x70]) + bytes([m[0x78], m[0x79], m[0xE8], m[0xE9]]) + bytes(m[0x84:0xD4]) + bytes(m[0x623A:0x65BA])


def make_inputs(seed, ticks):
    """Keys for a scenario: about every 10th tick one random key of KEYSET (a fixed list, stored in the output)."""
    rng = random.Random(seed)
    return [[t, rng.choice(KEYSET)] for t in range(1, ticks) if rng.random() < .1]


def run(name, inputs=None, pokes=None):
    sc = SCENARIOS[name]
    ticks = sc['ticks']
    inputs = inputs if inputs is not None else make_inputs(sc['seed'], ticks)
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    by_tick = {}
    for t, k in inputs:
        by_tick.setdefault(t, []).append(k)
    given = {}
    for t_, a_, v_ in (pokes or []):
        given.setdefault(t_, []).append((a_, v_))
    given = given if pokes is not None else None
    pending = list(sc['pokes']) if pokes is None else []
    applied = []
    out = {'name': name, 'seed': sc['seed'], 'ticks_requested': ticks, 'inputs': inputs, 'checkpoints': [], 'samples': [], 'events': {}, 'pokes': applied}
    st = {'t': 0, 'h': b'', 'done': False, 'wait': 0}
    step = mc.cpu.step

    def hook():
        pc = mc.cpu.pc
        if pc in MARKS and mc.m[0xE8] == 0:
            out['events'][MARKS[pc]] = out['events'].get(MARKS[pc], 0) + 1
        if pc == TICK_PC and mc.m[0xE8] == 0 and not st['done']:
            t = st['t']
            st['h'] = hashlib.sha256(st['h'] + state_bytes(mc.m)).digest()
            if t % 500 == 0:
                out['checkpoints'].append([t, st['h'].hex()])
            if t % 50 == 0:
                out['samples'].append([t] + [mc.m[a] for _, a in FIELDS])
            if t >= ticks:
                st['done'] = True
                out['ticks_done'] = t
                out['final'] = st['h'].hex()
            else:
                for k in by_tick.get(t, []):
                    mc.push_key(k)
                if given is not None:
                    for a_, v_ in given.get(t, []):
                        mc.m[a_] = v_
                        applied.append([t, a_, v_])
                else:
                    for p in list(pending):
                        kind, lvl, delay = p
                        if kind == 'eat_ghost':
                            # while ghost 1 is frightened (flag $8C) and not eyes ($8D), put Pac-Man on top of it (at most `delay` times, 150 ticks apart)
                            if t >= 150 * (st.get('eaten', 0) + 1) and mc.m[0x8C] and not mc.m[0x8D] and mc.m[0x58] != 0:
                                for a, v in ((0x60, mc.m[0x84]), (0x61, mc.m[0x85])):
                                    mc.m[a] = v
                                    applied.append([t, a, v])
                                st['eaten'] = st.get('eaten', 0) + 1
                                if st['eaten'] >= delay:
                                    pending.remove(p)
                            continue
                        if kind == 'energizers':
                            if t == delay:                      # every 7th plain dot of the maze map becomes an energizer (value 131)
                                for a in range(0x623A, 0x65BA):
                                    if mc.m[a] == 1 and (a - 0x623A) % 7 == 0:
                                        mc.m[a] = 131
                                        applied.append([t, a, 131])
                                pending.remove(p)
                            continue
                        if mc.m[0x55] == lvl:
                            st['wait'] = st.get('wait', 0) + 1
                            if st['wait'] >= delay:
                                mc.m[0x56] = 3                 # three items from the end of the level
                                applied.append([t, 0x56, 3])
                                pending.remove(p)
                                st['wait'] = 0
                st['t'] = t + 1
        return step()
    mc.cpu.step = hook
    guard = 0
    while not st['done'] and guard < 3000:
        guard += 1
        if mc.m[0xE8] != 0 and st['t'] > 0:        # game over: the demo has started again
            out['ticks_done'] = st['t']
            out['final'] = st['h'].hex()
            out['ended'] = 'game over'
            break
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    out.setdefault('ticks_done', st['t'])
    out.setdefault('final', st['h'].hex())
    return out


def run_all():
    return {'tick_pc': '$%04X' % TICK_PC, 'fields': [n for n, _ in FIELDS], 'keyset': list(KEYSET), 'scenarios': {n: run(n) for n in SCENARIOS}}


def check(stored):
    bad = []
    for n, s in stored['scenarios'].items():
        r = run(n, s['inputs'], s['pokes'])
        for key in ('checkpoints', 'samples', 'events', 'final', 'ticks_done'):
            if r[key] != s[key]:
                bad.append((n, key))
    return bad


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'check':
        bad = check(json.load(open('tests/golden/gameplay_traces.json')))
        print('MISMATCH' if bad else 'all scenarios replay identically', bad)
    else:
        path = sys.argv[1] if len(sys.argv) > 1 else 'tests/golden/gameplay_traces.json'
        res = run_all()
        json.dump(res, open(path, 'w'))
        for n, s in res['scenarios'].items():
            print(n, 'ticks', s['ticks_done'], s.get('ended', ''), 'events', s['events'], 'pokes', s['pokes'], 'final', s['final'][:16])
