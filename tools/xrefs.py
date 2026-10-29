"""Cross-reference analysis of reached code: which addresses does it read/write/point at, and which
unreached regions look like code-pointer tables? Usage: python -I tools/xrefs.py <body.bin> <origin-hex> <entry-hex>"""
import sys, os, collections
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import OPS, MODES, disassemble
from recdis import analyse

body = open(sys.argv[1], 'rb').read(); org = int(sys.argv[2], 16); entry = int(sys.argv[3], 16)
end = org + len(body)
code, labels, *_ = analyse(body, org, [entry])
reached = set(b for a, s in code.items() for b in range(a, a + s))
ins = {a: disassemble(body[a - org:a - org + s], a)[0] for a, s in code.items()}

# 1. absolute-operand data references into the program (not JSR/JMP)
refs = collections.defaultdict(set)   # target -> {'R','W'}
for a, (_, raw, text) in ins.items():
    mn = text.split()[0]; op = body[a - org]; mode = OPS[op][1]
    if mode in ('abs', 'abx', 'aby') and mn not in ('JSR', 'JMP'):
        t = raw[1] | raw[2] << 8
        if org <= t < end: refs[t].add('W' if mn in ('STA', 'STX', 'STY', 'INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR') else 'R')
tbl = sorted(refs)
unreached = [a for a in range(org, end) if a not in reached]
# group referenced addresses into clusters (gap < 0x40)
clusters = []
for t in tbl:
    if clusters and t - clusters[-1][-1] < 0x40: clusters[-1].append(t)
    else: clusters.append([t])
print(f"absolute data refs into program: {len(tbl)} distinct addresses in {len(clusters)} clusters; "
      f"in reached code: {sum(1 for t in tbl if t in reached)} (should be ~0 unless self-modifying)")
print("clusters (first-last, #addrs, modes):")
for c in clusters[:40]:
    m = ''.join(sorted(set(x for t in c for x in refs[t])))
    print(f"  {c[0]:04X}-{c[-1]:04X} n={len(c)} {m}")
if len(clusters) > 40: print(f"  ... {len(clusters)-40} more")

# 2. RTS-dispatch: PHA followed by PHA then RTS (return-address trick)
seq = sorted(ins)
rd = [a for a in seq if ins[a][2] == 'PHA' and a + 1 in ins and ins[a + 1][2] == 'PHA' and a + 2 in ins and ins[a + 2][2] == 'RTS']
print("PHA/PHA/RTS dispatch sites:", [f"{a:04X}" for a in rd])

# 3. self-modifying code: absolute stores whose target lies inside reached code
def owner(t):
    for a in range(t, t - 3, -1):
        if a in code and a + code[a] > t: return a, t - a
patched = collections.Counter()
for a, s_ in code.items():
    mn, mode = OPS[body[a - org]]
    if mode in ('abs', 'abx', 'aby') and mn in ('STA', 'STX', 'STY', 'INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR'):
        t = body[a - org + 1] | body[a - org + 2] << 8
        if t in reached: patched[owner(t)] += 1
print(f"self-modifying code: {len(set(o for o, _ in patched))} instructions patched, "
      f"{sum(1 for _, off in patched if off > 0)} operand bytes, {sum(1 for _, off in patched if off == 0)} opcode bytes")
print("most-patched sites (instr+offset: writers):", [(f"{o:04X}+{off}", n) for (o, off), n in patched.most_common(6)])
print(f"unreached bytes: {len(unreached)}; bytes inside absolute-referenced clusters: {sum(c[-1]-c[0]+1 for c in clusters)}")
