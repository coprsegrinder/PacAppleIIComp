"""List every instruction of the known code that reads or writes given zero-page variables (development tool).
Usage: python -I tools/zp_refs.py <hex> [<hex> ...]       e.g.  python -I tools/zp_refs.py 78 79 7A E9 80
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import OPS
from recdis import analyse
from static_plus import table_targets

ORIGIN = 0x4000
READS = {'LDA', 'LDX', 'LDY', 'CMP', 'CPX', 'CPY', 'AND', 'ORA', 'EOR', 'ADC', 'SBC', 'BIT'}
WRITES = {'STA', 'STX', 'STY'}
RMW = {'INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR'}
ZP_MODES = {'zp', 'zpx', 'zpy', 'izx', 'izy'}


def references(body, code, variables):
    """{variable: [(address, mnemonic, kind)]} with kind 'read', 'write' or 'modify' (zero-page and indirect-pointer uses)."""
    out = {v: [] for v in variables}
    for a, n in sorted(code.items()):
        mn, mode = OPS[body[a - ORIGIN]]
        if mode in ZP_MODES and n == 2:
            v = body[a - ORIGIN + 1]
            if v in out:
                kind = 'read' if mn in READS else 'write' if mn in WRITES else 'modify' if mn in RMW else 'other'
                if mode in ('izx', 'izy'):
                    kind = 'pointer'
                out[v].append((a, mn, kind))
    return out


def main():
    variables = [int(a, 16) for a in sys.argv[1:]]
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    code, *_ = analyse(body, ORIGIN, [0x4027] + table_targets(body))
    for v, refs in references(body, code, variables).items():
        print('$%02X: %d reads, %d writes, %d modifies' % (v, *(sum(1 for r in refs if r[2] == k) for k in ('read', 'write', 'modify'))))
        print('    ' + '  '.join('%04X:%s%s' % (a, mn, {'read': '', 'write': '!', 'modify': '~', 'pointer': '*', 'other': '?'}[k]) for a, mn, k in refs))


if __name__ == '__main__':
    main()
