"""Validate tools/a2tc_collect.py against a2run's own collection (TASK-029h2).
Usage: python -I tools/a2tc_validate.py <a2run.json> <branch-limit>
Runs the game image from the verified extract in our oracle machine with the collector until <branch-limit> control-flow instructions,
and compares branch targets, branches and generations with the a2run file for the same run (attract mode, no keys), game area only."""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey
from a2tc_collect import Collector

ref = json.load(open(sys.argv[1]))
limit = int(sys.argv[2])
body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
mc = Machine()
mc.init_text_screen()
mc.load(0x1000, body)
mc.cpu.pc = 0x1000
col = Collector(mc, limit=limit)
while not col.stop:
    try:
        mc.run(2000)
    except NeedKey:
        break
col.finish()
mine = col.data()
G = lambda a: 0x1000 <= a < 0xAB00
rt = {t for t in ref['BranchTargets'] if G(t)}
mt = {t for t in mine['BranchTargets'] if G(t)}
print('branch targets  ref %d mine %d  only-ref %s only-mine %s' % (len(rt), len(mt), sorted(rt - mt)[:10], sorted(mt - rt)[:10]))
re_ = {int(k): set(v) for k, v in ref['Branches'].items() if G(int(k))}
me_ = {int(k): set(v) for k, v in mine['Branches'].items() if G(int(k))}
print('branch origins  ref %d mine %d  only-ref %s only-mine %s' % (len(re_), len(me_), sorted(set(re_) - set(me_))[:10], sorted(set(me_) - set(re_))[:10]))
bad = [k for k in re_ if k in me_ and re_[k] != me_[k]]
print('origins with different targets:', len(bad), [hex(k) for k in bad[:8]])
print('generations ref %d mine %d' % (len(ref['generations']), len(mine['generations'])))
for i, (a, b) in enumerate(zip(ref['generations'], mine['generations'])):
    ca = {(c['addr'], tuple(c['bytes'])) for c in a['code']}
    cb = {(c['addr'], tuple(c['bytes'])) for c in b['code']}
    print(' gen', i, 'segments ref', len(a['code']), 'mine', len(b['code']), 'identical' if ca == cb else 'DIFFERENT')
print('branches counted: ref', ref['BaseStats']['limit'], 'mine', mine['BaseStats']['limit'])
