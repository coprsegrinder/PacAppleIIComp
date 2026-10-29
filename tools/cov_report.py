"""Summarise scratch/cov.json (from trace_coverage.py) against the static disassembly; writes bulk detail to scratch/.
Usage: python -I tools/cov_report.py   (needs scratch/extract/PAC-MAN.1000.bin and scratch/cov.json)
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from recdis import analyse

LO, HI = 0x4000, 0xAA00


def span(runs):
    s = set()
    for a, b in runs:
        s.update(range(a, b + 1))
    return s


def fmt_runs(addrs):
    out, a = [], sorted(addrs)
    for x in a:
        if out and x == out[-1][1] + 1:
            out[-1][1] = x
        else:
            out.append([x, x])
    return out


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    cov = json.load(open('scratch/cov.json'))
    code, *_ = analyse(body, 0x4000, [0x4027])
    static = set()
    for a, s in code.items():
        static.update(range(a, a + s))
    ex = {a for a in span(cov['exec']) if LO <= a < HI}
    rd, wr = span(cov['read']), span(cov['write'])
    writer = {int(k): v for k, v in cov['writer'].items()}
    print('keys', cov['keys'], 'cycles', cov['cycles'])
    print('executed (game area):', len(ex), '| static-reached:', len(static))
    print('executed AND static:', len(ex & static), '| executed, NOT in static:', len(ex - static),
          '| static, not executed in this run:', len(static - ex))
    sm = sorted(a for a in ex if a in wr)
    print('executed bytes that were written during the run (self-modifying):', len(sm))
    for a, b in fmt_runs(sm)[:40]:
        wp = sorted({writer[x] for x in range(a, b + 1) if x in writer})
        print('  $%04X-$%04X written by %s; executed instr start(s) in static=%s' %
              (a, b, ','.join('$%04X' % p for p in wp[:6]), all(x in static for x in range(a, b + 1))))
    dyn = ex - static
    print('executed but NOT found statically: %d runs (full list in scratch/cov_detail.txt), first 8:' % len(fmt_runs(dyn)))
    for a, b in fmt_runs(dyn)[:8]:
        print('  $%04X-$%04X' % (a, b))
    tbl = [a for a in range(0x623A, 0x65BA) if a in writer]
    print('writers of $623A-$65B9:', sorted({'$%04X' % writer[a] for a in tbl}))
    rdnc = {a for a in rd if LO <= a < HI} - ex
    print('game-area bytes READ as data (not executed):', len(rdnc), 'in', len(fmt_runs(rdnc)), 'runs')
    wr_game = {a for a in wr if LO <= a < HI}
    print('game-area bytes written:', len(wr_game))
    allb = set(range(LO, HI))
    untouched = allb - ex - rdnc - wr_game
    print('game-area bytes neither executed, read nor written in this run:', len(untouched), '(%.1f%%)' % (100 * len(untouched) / len(allb)))
    with open('scratch/cov_detail.txt', 'w') as f:
        f.write('executed-not-static runs\n' + '\n'.join('%04X-%04X' % tuple(r) for r in fmt_runs(dyn)))
        f.write('\n\nread-as-data runs (not executed)\n' + '\n'.join('%04X-%04X' % tuple(r) for r in fmt_runs(rdnc)))
        f.write('\n\nself-modified\n' + '\n'.join('%04X-%04X' % tuple(r) for r in fmt_runs(sm)))


if __name__ == '__main__':
    main()
