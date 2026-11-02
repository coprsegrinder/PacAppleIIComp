"""Record apple2tc run data (tools/a2tc_collect.py) for one recorded scenario, replaying its keys and pokes in the oracle machine.
Usage: python -I tools/a2tc_scenarios.py <X>      X = A|B|C|D (stored in tests/golden/gameplay_traces.json) or the letters/names of the other
       scratch/ticks_X.bin recordings (their trailer holds the keys and pokes: F G H J K L M P Q R S, L04.., T00, N00, B10) or DEMO (attract mode)
       -> scratch/a2tc_run_<X>.json   (original code bytes inside: git-ignored scratch/)
P, Q, R, S start a TWO-player game (key 2) like tools/record_tick_calls.py."""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
from apple2 import Machine, NeedKey
from a2tc_collect import Collector

COLLECTORS = []


def factory(cov=False):
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    col = Collector(mc)
    COLLECTORS.append(col)
    mc.run(10_000_000, stop_pc=0x4027)
    return mc


def read_trailer(path):
    d = open(path, 'rb').read()
    n = struct.unpack('<I', d[4:8])[0]
    pos = 44 + n * 1010
    k = struct.unpack('<I', d[pos:pos + 4])[0]
    pos += 4
    inputs = [[struct.unpack('<II', d[pos + 8 * i:pos + 8 * i + 8])[0], struct.unpack('<II', d[pos + 8 * i:pos + 8 * i + 8])[1] & 0x7F] for i in range(k)]
    pos += 8 * k
    k = struct.unpack('<I', d[pos:pos + 4])[0]
    pos += 4
    pokes = [list(struct.unpack('<III', d[pos + 12 * i:pos + 12 * i + 12])) for i in range(k)]
    return n - 1, inputs, pokes


def demo(col_list):
    from play import do_step
    mc = factory()
    for st in ('key:1', 'key:cr'):
        do_step(mc, st, lambda *_: None)
    n = [0]
    step = mc.cpu.step

    def hook():
        if mc.cpu.pc == G.TICK_PC and mc.m[0xE8] != 0:
            n[0] += 1
        return step()
    mc.cpu.step = hook
    while n[0] < 4000:
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    return n[0]


name = sys.argv[1]
if name == 'DEMO':
    ticks = demo(COLLECTORS)
    out = {'ticks_done': ticks, 'final': ''}
    stored_final = ''
else:
    G.factory = factory
    if name in 'ABCD' and len(name) == 1:
        stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][name]
        G.SCENARIOS[name]['ticks'] = stored.get('ticks_done', G.SCENARIOS[name]['ticks'])
        inputs, pokes, stored_final = stored['inputs'], stored['pokes'], stored['final'][:16]
    else:
        n, inputs, pokes = read_trailer('scratch/ticks_%s.bin' % name)
        G.SCENARIOS[name] = dict(seed=0, ticks=n, pokes=[])
        stored_final = ''
        if name in ('P', 'Q', 'R', 'S'):
            from play import do_step

            def two_players(mc):
                for st in ('key:sp', '3_000_000', 'key:2', '3_000_000', 'key:cr', '1_000_000'):
                    do_step(mc, st, lambda *_: None)
            G.start_game = two_players
    out = G.run(name, inputs, pokes)
col = COLLECTORS[0]
col.save('scratch/a2tc_run_%s.json' % name)
print(name, 'ticks', out.get('ticks_done'), 'final', out.get('final', '')[:16], 'stored final', stored_final, 'branches', col.icount,
      'targets', len(col.targets), 'generations', len(col.generations), flush=True)
