"""Merge several coverage JSON files (exec run lists) and compare with the static code map (entry + dispatch table).
Usage: python -I tools/cov_union.py file.json [file.json ...]   (reads scratch/extract/PAC-MAN.1000.bin; writes scratch/unexecuted.txt)
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from recdis import analyse
from static_plus import table_targets, bytes_of

LO, HI = 0x4000, 0xAA00


def exec_set(path):
    ex = set()
    for a, b in json.load(open(path))['exec']:
        ex.update(range(max(a, LO), min(b, HI - 1) + 1))
    return ex


def to_runs(addrs):
    out = []
    for x in sorted(addrs):
        if out and x == out[-1][1] + 1:
            out[-1][1] = x
        else:
            out.append([x, x])
    return out


def compare(executed, static):
    return {'executed': len(executed), 'static': len(static), 'executed_and_static': len(executed & static),
            'executed_not_static': sorted(executed - static), 'static_not_executed': sorted(static - executed)}


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    code, *_ = analyse(body, LO, [0x4027] + table_targets(body))
    static = bytes_of(code)
    total = set()
    for p in sys.argv[1:]:
        e = exec_set(p)
        print('%-24s executed %5d bytes (new vs previous files: %d)' % (os.path.basename(p), len(e), len(e - total)))
        total |= e
    r = compare(total, static)
    print('union executed: %d | static code: %d (%.1f%% of program) | executed and static: %d | executed but NOT static: %d | static not executed: %d (%.1f%% of static)' % (
        r['executed'], r['static'], 100 * r['static'] / (HI - LO), r['executed_and_static'], len(r['executed_not_static']),
        len(r['static_not_executed']), 100 * len(r['static_not_executed']) / r['static']))
    if r['executed_not_static']:
        print('  contradictions:', [('%04X' % a, '%04X' % b) for a, b in to_runs(r['executed_not_static'])][:20])
    with open('scratch/unexecuted.txt', 'w') as f:
        for a, b in to_runs(r['static_not_executed']):
            f.write('%04X-%04X %d\n' % (a, b, b - a + 1))
    big = sorted(to_runs(r['static_not_executed']), key=lambda r_: r_[0] - r_[1])[:10]
    print('largest unexecuted static-code runs:', [('%04X' % a, b - a + 1) for a, b in big], 'in', len(to_runs(r['static_not_executed'])), 'runs')


if __name__ == '__main__':
    main()
