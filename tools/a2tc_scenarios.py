"""Record apple2tc run data (tools/a2tc_collect.py) for one golden scenario of tests/golden/gameplay_traces.json, replaying its stored keys and pokes.
Usage: python -I tools/a2tc_scenarios.py <A|B|C|D>   -> scratch/a2tc_run_<X>.json   (original code bytes inside: git-ignored scratch/)"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
from apple2 import Machine
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


G.factory = factory
name = sys.argv[1]
stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][name]
G.SCENARIOS[name]['ticks'] = stored['ticks_done'] if 'ticks_done' in stored else G.SCENARIOS[name]['ticks']
out = G.run(name, stored['inputs'], stored['pokes'])
col = COLLECTORS[0]
col.save('scratch/a2tc_run_%s.json' % name)
print(name, 'ticks', out.get('ticks_done'), 'final', out.get('final', '')[:16], 'stored final', stored['final'][:16], 'branches', col.icount,
      'targets', len(col.targets), 'generations', len(col.generations))
