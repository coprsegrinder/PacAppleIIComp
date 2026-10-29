"""Run the extracted PAC-MAN headless with coverage on (uses original data from scratch/; development only).
Usage: python -I tools/trace_coverage.py [keys] [max_instr_per_stage]    e.g.  python -I tools/trace_coverage.py 1N 20000000
Types each key when the game next waits for one, then runs until the next wait or the instruction limit.
Writes scratch/cov.json (executed/read/written address lists + last writer) and prints a summary.
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey
from cpu6502 import CPUError

BASE = 0x4000


def runs(flags, lo=0, hi=0x10000):
    out, start = [], None
    for a in range(lo, hi):
        if flags[a]:
            if start is None:
                start = a
        elif start is not None:
            out.append((start, a - 1)); start = None
    if start is not None:
        out.append((start, hi - 1))
    return out


def main():
    keys = sys.argv[1] if len(sys.argv) > 1 else '1N'
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 20_000_000
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    mc.enable_coverage()
    mc.run(10_000_000, stop_pc=0x4027)          # let the startup stub relocate the program, then forget its writes
    assert mc.cpu.pc == 0x4027
    mc.cov['write'][:] = bytes(65536)
    mc.cov['writer'][:] = type(mc.cov['writer'])('H', [0]) * 65536
    stages = []
    pending = list(keys) + [None]
    while pending:
        k = pending.pop(0)
        if k is not None:
            mc.push_key(k)
        try:
            n = mc.run(limit)
            stages.append(('limit', n, mc.cpu.pc))
            break
        except NeedKey:
            stages.append(('needkey', None, mc.cpu.pc))
        except CPUError as e:
            stages.append(('cpuerror: %s' % e, None, mc.cpu.pc))
            break
    for s in stages:
        print('stage:', s[0], 'pc=%04X' % s[2])
    print('cycles', mc.cpu.cycles, 'rom calls', mc.rom_calls, 'unknown io', mc.unknown_io, 'speaker clicks', len(mc.speaker))
    c = mc.cov
    g = lambda f: sum(f[BASE:0xAA00])
    print('executed bytes in game $4000-$A9FF:', g(c['exec']), 'of', 0xAA00 - BASE,
          '| instructions starts:', g(c['opstart']))
    json.dump({'exec': runs(c['exec']), 'read': runs(c['read']), 'write': runs(c['write']),
               'opstart': [a for a in range(65536) if c['opstart'][a]],
               'writer': {str(a): c['writer'][a] for a in range(65536) if c['write'][a]},
               'keys': keys, 'cycles': mc.cpu.cycles, 'stages': stages},
              open('scratch/cov.json', 'w'))
    print('\n'.join(l for l in mc.text_lines() if l))


if __name__ == '__main__':
    main()
