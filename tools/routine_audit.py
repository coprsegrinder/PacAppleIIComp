"""TASK-033-R: routine-by-routine audit data. Finds every routine entry of the original program (the entry $4027, every JSR target, the 48 dispatch-table targets, JMP targets that
are not inside another routine's branch range), measures each one (bytes, executed bytes from the coverage files, number of callers) and searches the port sources and tests for
its address. Writes scratch/audit.json (original facts, git-ignored) and prints a table.
Usage: python -I tools/routine_audit.py"""
import json, os, re, sys, glob
sys.path.insert(0, os.path.dirname(__file__))
from recdis import analyse
from static_plus import table_targets
from cov_union import exec_set
from dis6502 import OPS, MODES

LO = 0x4000
body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
tt = table_targets(body)
code, labels, indirect, outside, overlaps = analyse(body, LO, [0x4027] + tt)

jsr_t, jmp_t, callers = set(), set(), {}
for a, s in code.items():
    op = body[a - LO]
    mn, mode = OPS[op]
    if mn in ('JSR', 'JMP') and mode == 'abs':
        t = body[a - LO + 1] | body[a - LO + 2] << 8
        if LO <= t < LO + len(body):
            (jsr_t if mn == 'JSR' else jmp_t).add(t)
            callers.setdefault(t, []).append((a, mn))
entries = sorted(set([0x4027]) | jsr_t | set(tt) | jmp_t)

# extent of a routine: follow branches and JMPs inside, not JSRs; stop at RTS/JMP/RTI
def extent(e):
    seen, work = set(), [e]
    while work:
        a = work.pop()
        while a in code and a not in seen:
            seen.add(a)
            op = body[a - LO]
            mn, mode = OPS[op]
            if mode == 'rel':
                work.append((a + 2 + (body[a - LO + 1] - 256 if body[a - LO + 1] > 127 else body[a - LO + 1])) & 0xFFFF)
            if mn == 'JMP' and mode == 'abs':
                t = body[a - LO + 1] | body[a - LO + 2] << 8
                if t not in entries or t == e:
                    work.append(t)
                break
            if mn in ('RTS', 'RTI', 'BRK'):
                break
            a += code[a]
    return seen

ex = set()
for p in ['cov', 'cov_auto1', 'cov_auto2', 'cov_cartoons', 'cov_custom', 'cov_play', 'cov_rank', 'cov_close', 'cov_close2', 'cov_close3', 'cov_close4_A', 'cov_close4_B', 'cov_close5_6', 'cov_close5_C', 'cov_close5_D', 'cov_close5_E']:
    f = 'scratch/%s.json' % p
    if os.path.exists(f):
        ex |= exec_set(f)

srcs = {}
for pat in ('port/src/**/*.cpp', 'port/src/**/*.h', 'port/tests/*.cpp', 'port/tests/*.h', 'tools/gen_*.py'):
    for f in glob.glob(pat, recursive=True):
        srcs[f.replace('\\', '/')] = open(f, encoding='utf-8', errors='replace').read()

def mentions(addr):
    pat = re.compile(r'(\$|0x|\b)%04X\b' % addr, re.I)
    out = {}
    for f, t in srcs.items():
        n = len(pat.findall(t))
        if n:
            out[f] = n
    return out

rows = []
for e in entries:
    ext = extent(e)
    nb = sum(code[a] for a in ext)
    ne = sum(1 for a in ext for b in range(a, a + code[a]) if b in ex)
    m = mentions(e)
    rows.append({'addr': '%04X' % e, 'kind': ('table' if e in tt else '') + ('jsr' if e in jsr_t else '') + ('jmp' if e in jmp_t else '') + ('entry' if e == 0x4027 else ''),
                 'bytes': nb, 'exec': ne, 'callers': len(callers.get(e, [])), 'caller_addrs': ['%04X' % a for a, _ in callers.get(e, [])][:6],
                 'src': {k.split('/')[-1]: v for k, v in m.items() if '/src/' in k or 'gen_' in k}, 'tests': {k.split('/')[-1]: v for k, v in m.items() if '/tests/' in k}})
json.dump(rows, open('scratch/audit.json', 'w'), indent=1)
print('entries', len(rows), 'jsr', len(jsr_t), 'table', len(tt), 'jmp', len(jmp_t))
none = [r for r in rows if not r['src']]
print('no mention in port/src:', len(none), '| no mention in src or tests:', len([r for r in rows if not r['src'] and not r['tests']]))
for r in rows:
    print(r['addr'], r['kind'].ljust(8), '%4dB %4d exec' % (r['bytes'], r['exec']), 'callers', r['callers'], 'SRC', ','.join(r['src']) or '-', '| TESTS', ','.join(r['tests']) or '-')
