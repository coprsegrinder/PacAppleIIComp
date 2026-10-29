"""Recursive disassembly with extra entry points: the 48-entry dispatch table at $8DBE (found via the patched JSR at $8CA6)
and, optionally, every instruction start executed in a coverage run (scratch/cov.json). Development tool; reads original data from scratch/.
Usage: python -I tools/static_plus.py [body.bin] [cov.json]
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from recdis import analyse

ORIGIN = 0x4000
TABLE, COUNT = 0x8DBE, 48


def table_targets(body, origin=ORIGIN, table=TABLE, count=COUNT):
    out = []
    for i in range(count):
        o = table - origin + 2 * i
        out.append(body[o] | body[o + 1] << 8)
    return out


def bytes_of(code):
    s = set()
    for a, n in code.items():
        s.update(range(a, a + n))
    return s


def main():
    body = open(sys.argv[1] if len(sys.argv) > 1 else 'scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    covp = sys.argv[2] if len(sys.argv) > 2 else 'scratch/cov.json'
    end = ORIGIN + len(body)
    tt = table_targets(body)
    assert all(ORIGIN <= t < end for t in tt), 'table target outside program: table size guess is wrong'
    dyn = [a for a in json.load(open(covp))['opstart'] if ORIGIN <= a < end] if os.path.exists(covp) else []
    results = {}
    for name, entries in [('baseline (entry $4027)', [0x4027]),
                          ('+ dispatch table (48 targets)', [0x4027] + tt),
                          ('+ table + executed instruction starts', [0x4027] + tt + dyn)]:
        code, labels, indirect, outside, overlaps = analyse(body, ORIGIN, entries)
        b = bytes_of(code)
        results[name] = b
        print('%-42s %5d instr %6d bytes (%4.1f%%) overlaps %d outside %s' % (
            name, len(code), len(b), 100 * len(b) / len(body), len(overlaps), sorted('%04X' % x for x in outside)))
        print('    indirect jumps:', ['%04X' % x for x in indirect])
    full = results['+ table + executed instruction starts']
    ex = set()
    for a in dyn:
        ex.add(a)
    code, *_ = analyse(body, ORIGIN, [0x4027] + tt + dyn)
    bad = [a for a in dyn if a not in code]
    print('executed instruction starts:', len(dyn), '| not decoded identically as instruction starts:', len(bad))
    only_tt = results['+ dispatch table (48 targets)'] - results['baseline (entry $4027)']
    print('code bytes added by the dispatch table alone:', len(only_tt))
    json.dump({'code': {'%04X' % a: n for a, n in code.items()}}, open('scratch/static_plus.json', 'w'))


if __name__ == '__main__':
    main()
