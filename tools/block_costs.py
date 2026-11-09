"""TASK-034-TIMING: the static cycle cost of the original's code, block by block (development tool; original data from scratch/).
Prints, for a range of the game's code ($4000 + offset in PAC-MAN.1000.bin), the straight-line blocks between branch targets and branches: for every block its address range, its cost (every
instruction at its base cost; a conditional branch counts as NOT taken), and at the branch the extra cycles when it is taken (+1, +2 when the target is on another page). Indexed reads
(abs,X / abs,Y / (zp),Y) are listed with a P: they cost one more when the index carries over a page. A JSR counts 6; the callee is separate.
Usage: python -I tools/block_costs.py <first-hex> <last-hex>        e.g. 8763 8866"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import disassemble, OPS, MODES

BASE = {'imm': 2, 'zp': 3, 'zpx': 4, 'zpy': 4, 'abs': 4, 'abx': 4, 'aby': 4, 'izx': 6, 'izy': 5}
RMW = {'ASL', 'LSR', 'ROL', 'ROR', 'INC', 'DEC'}
STORE = {'STA', 'STX', 'STY'}
BRANCH = {'BCC', 'BCS', 'BEQ', 'BNE', 'BPL', 'BMI', 'BVC', 'BVS'}


def cost(mn, mode):
    if mn in BRANCH:
        return 2
    if mn == 'JSR':
        return 6
    if mn == 'JMP':
        return 3 if mode == 'abs' else 5
    if mn == 'RTS' or mn == 'RTI':
        return 6
    if mn == 'BRK':
        return 7
    if mode in ('imp', 'acc'):
        return {'PHA': 3, 'PHP': 3, 'PLA': 4, 'PLP': 4}.get(mn, 2)
    c = BASE[mode]
    if mn in RMW:
        return {'zp': 5, 'zpx': 6, 'abs': 6, 'abx': 7}[mode]
    if mn in STORE:
        return {'zp': 3, 'zpx': 4, 'zpy': 4, 'abs': 4, 'abx': 5, 'aby': 5, 'izx': 6, 'izy': 6}[mode]
    return c


def main():
    lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
    data = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    ins = disassemble(data, 0x4000, lo - 0x4000, hi - lo + 3)
    ins = [i for i in ins if i[0] <= hi]
    targets = set()
    for a, raw, text in ins:
        p = text.split()
        if p[0] in BRANCH or p[0] in ('JMP',):
            if p[1].startswith('$'):
                targets.add(int(p[1][1:5], 16))
    block, start, total = [], None, 0
    for a, raw, text in ins:
        p = text.split()
        mn = p[0]
        mode = OPS[raw[0]][1] if raw[0] in OPS else 'imp'
        if start is None or a in targets:
            if start is not None and block:
                print('  block %04X-%04X  %3d cycles (falls through)' % (start, a - 1, total))
            start, total, block = a, 0, []
        if a in targets and a != start:
            pass
        c = cost(mn, mode)
        total += c
        block.append(a)
        flag = ' P' if mode in ('abx', 'aby', 'izy') and mn not in STORE and mn not in RMW else ''
        extra = ''
        if mn in BRANCH:
            tgt = int(p[1][1:5], 16)
            nxt = a + 2
            extra = '   taken +%d -> %04X' % (1 + ((nxt >> 8) != (tgt >> 8)), tgt)
        if mn == 'JMP' and p[1].startswith('$'):
            extra = '   -> %s' % p[1]
        print('%04X  %-14s %2d%s%s' % (a, text, c, flag, extra))
        if mn in BRANCH or mn in ('JMP', 'RTS', 'RTI'):
            print('  block %04X-%04X  %3d cycles%s' % (start, a, total, ' (to here; branch not taken)' if mn in BRANCH else ''))
            start, total, block = None, 0, []


if __name__ == '__main__':
    main()
