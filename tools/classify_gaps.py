"""Classify the program bytes that static analysis (with the dispatch-table entries) does not mark as code.
Evidence per gap: bytes read as data at runtime (scratch/cov.json), pointers into the gap from code (absolute operands and
LDA/LDX/LDY #lo + #hi immediate pairs), and zero fill. Verdicts are heuristic labels, not proof.
Usage: python -I tools/classify_gaps.py   -> prints summary, writes scratch/gaps.txt
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import OPS
from recdis import analyse
from static_plus import table_targets, ORIGIN

ABS_MODES = ('abs', 'abx', 'aby')


def gaps_of(code_bytes, lo, hi):
    out, s = [], None
    for a in range(lo, hi):
        if a not in code_bytes:
            if s is None:
                s = a
        elif s is not None:
            out.append((s, a - 1)); s = None
    if s is not None:
        out.append((s, hi - 1))
    return out


def pointers_from_code(body, code, lo, hi):
    """Addresses (inside [lo,hi)) referenced by absolute operands or by adjacent #lo/#hi immediate pairs."""
    refs = {}
    seq = sorted(code.items())
    for a, n in seq:
        op = body[a - ORIGIN]
        mn, mode = OPS[op]
        if mode in ABS_MODES and mn not in ('JSR', 'JMP'):
            t = body[a - ORIGIN + 1] | body[a - ORIGIN + 2] << 8
            if lo <= t < hi:
                refs.setdefault(t, []).append(a)
    imms = [(a, body[a - ORIGIN + 1]) for a, n in seq if OPS[body[a - ORIGIN]][1] == 'imm' and OPS[body[a - ORIGIN]][0] in ('LDA', 'LDX', 'LDY')]
    for i, (a, v) in enumerate(imms):
        for b, w in imms[i + 1:i + 3]:
            if b - a <= 8:
                for t in (v | w << 8, w | v << 8):
                    if lo <= t < hi:
                        refs.setdefault(t, []).append(a)
    return refs


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    lo, hi = ORIGIN, ORIGIN + len(body)
    code, *_ = analyse(body, ORIGIN, [0x4027] + table_targets(body))
    cb = set()
    for a, n in code.items():
        cb.update(range(a, a + n))
    cov = json.load(open('scratch/cov.json'))
    rd = set()
    for a, b in cov['read']:
        rd.update(range(max(a, lo), min(b, hi - 1) + 1))
    refs = pointers_from_code(body, code, lo, hi)
    gaps = gaps_of(cb, lo, hi)
    rows, tot = [], {'code': len(cb), 'zero': 0, 'data-read': 0, 'data-referenced': 0, 'unknown': 0}
    for a, b in gaps:
        n = b - a + 1
        chunk = body[a - ORIGIN:b - ORIGIN + 1]
        zeros = chunk.count(0)
        nread = sum(1 for x in range(a, b + 1) if x in rd)
        nref = sum(1 for t in refs if a <= t <= b)
        if zeros == n:
            v = 'zero'
        elif nread:
            v = 'data-read'
        elif nref:
            v = 'data-referenced'
        else:
            v = 'unknown'
        tot[v] += n
        rows.append((n, a, b, zeros, nread, nref, v))
    print('total %d bytes | code %d (%.1f%%)' % (len(body), len(cb), 100 * len(cb) / len(body)))
    for k in ('zero', 'data-read', 'data-referenced', 'unknown'):
        print('  %-16s %6d bytes (%4.1f%%)' % (k, tot[k], 100 * tot[k] / len(body)))
    print('gaps: %d' % len(rows))
    rows.sort(reverse=True)
    print('largest 15 gaps (size start-end zeros read refs verdict):')
    for n, a, b, z, r, f, v in rows[:15]:
        print('  %5d $%04X-$%04X zeros=%d read=%d refs=%d %s' % (n, a, b, z, r, f, v))
    unk = [r for r in rows if r[6] == 'unknown']
    print('unknown gaps: %d, largest:' % len(unk), ['$%04X(%d)' % (r[1], r[0]) for r in sorted(unk, reverse=True)[:12]])
    with open('scratch/gaps.txt', 'w') as f:
        for n, a, b, z, r, fr, v in sorted(rows, key=lambda r: r[1]):
            f.write('%04X-%04X size=%d zeros=%d read=%d refs=%d %s\n' % (a, b, n, z, r, fr, v))


if __name__ == '__main__':
    main()
