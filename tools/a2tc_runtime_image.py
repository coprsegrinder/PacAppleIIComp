"""TASK-029h3: the game as it is AFTER its own relocation, for apple2tc.
The program file loads at $1000 and moves itself to $4000 (entry $4027). apple2tc analyses the loaded image, so the run data must describe the code where it
runs: this tool (1) runs the verified program to its entry in the oracle machine and writes scratch/a2tc_PAC-MAN_4000.b33 (load $4000, $6A00 bytes) and
(2) rewrites a merged run-data file for it: only branch edges/targets and code segments inside $4000-$A9FF, start registers at $4027.
Usage: python -I tools/a2tc_runtime_image.py in.json out.json"""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import play

LO, HI = 0x4000, 0xAA00
mc = play.make_machine()
assert mc.cpu.pc == 0x4027
img = bytes(mc.m[LO:HI])
open('scratch/a2tc_PAC-MAN_4000.b33', 'wb').write(struct.pack('<HH', LO, HI - LO) + img)
regs = {'a': mc.cpu.a, 'pc': 0x4027, 'sp': mc.cpu.sp, 'status': mc.cpu.p, 'x': mc.cpu.x, 'y': mc.cpu.y}
d = json.load(open(sys.argv[1]))
inside = lambda a: LO <= a < HI
out = {'BaseStats': dict(d['BaseStats'], startRegs=regs),
       'BranchTargets': [t for t in d['BranchTargets'] if inside(t)],
       'Branches': {k: [t for t in v if inside(t)] for k, v in d['Branches'].items() if inside(int(k))},
       'generations': [{'regs': regs, 'code': []}]}
out['Branches'] = {k: v for k, v in out['Branches'].items() if v}
for g in d['generations'][1:]:
    code = []
    for c in g['code']:
        a, b = c['addr'], c['addr'] + len(c['bytes'])
        a2, b2 = max(a, LO), min(b, HI)
        if a2 < b2:
            code.append({'addr': a2, 'bytes': c['bytes'][a2 - a:b2 - a]})
    if code:
        out['generations'].append({'regs': regs, 'code': code})
json.dump(out, open(sys.argv[2], 'w'), separators=(',', ':'))
print('image %d bytes at $%04X; %d targets, %d branch origins, %d generations' % (len(img), LO, len(out['BranchTargets']), len(out['Branches']), len(out['generations'])))
