"""Truth tables of the 48 direction routines of the ghost logic (development tool; runs the original code on the Python oracle).
Each routine takes A = current direction (0-3), X/Y = target position (from $60/$61 or a corner) and compares with the ghost position in $70/$71;
it returns A = new direction. This tool runs every routine on a grid of inputs and prints which inputs each routine's answer depends on.
Usage: python -I tools/ghost_routines.py        -> prints a summary, writes scratch/ghost_routines.json
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine

GHOST = (100, 100)                       # ghost position ($70, $71)
XS = (60, 100, 140)                      # target X relative to the ghost: left / same / right
YS = (10, 34, 60, 100, 140)              # target Y: includes $22 = 34 (constant used by some routines) and 100 (= ghost Y)
DIRS = (0, 1, 2, 3)


def run_routine(mc, address, direction, x, y):
    """Call `address` with A=direction, X=x, Y=y and the ghost at GHOST; returns (A, X, Y) after RTS, or None if it did not return."""
    m = mc.m
    m[0x70], m[0x71] = GHOST
    m[0x0300:0x0303] = bytes([0x20, address & 0xFF, address >> 8])          # JSR address
    m[0x0303] = 0xEA
    cpu = mc.cpu
    cpu.pc, cpu.a, cpu.x, cpu.y, cpu.sp = 0x0300, direction, x, y, 0xFD
    try:
        mc.run(200, stop_pc=0x0303)
    except Exception:
        return None
    return (cpu.a, cpu.x, cpu.y) if cpu.pc == 0x0303 else None


def table(mc, address):
    return {(d, x, y): run_routine(mc, address, d, x, y) for d in DIRS for x in XS for y in YS}


def dependency(tbl):
    """Which of (direction, x, y) change the answer A, holding the others fixed? -> string like 'dir,x,y'."""
    deps = []
    for pos, name in ((0, 'dir'), (1, 'x'), (2, 'y')):
        changes = False
        for key, val in tbl.items():
            for other in tbl:
                if all(other[i] == key[i] for i in range(3) if i != pos) and other[pos] != key[pos] and tbl[other][0] != val[0]:
                    changes = True
                    break
            if changes:
                break
        if changes:
            deps.append(name)
    return ','.join(deps) if deps else 'constant'


def outputs_by_dir(tbl):
    return {d: sorted({v[0] for k, v in tbl.items() if k[0] == d and v is not None}) for d in DIRS}


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    words = [body[0x8DBE - 0x4000 + 2 * i] | body[0x8DBF - 0x4000 + 2 * i] << 8 for i in range(48)]
    index = body[0x8CBE - 0x4000:0x8CBE - 0x4000 + 256]
    uses = [sum(1 for v in index if v == r) for r in range(48)]
    mc = Machine()
    mc.load(0x4000, body)
    rows, sigs = [], {}
    for r, addr in enumerate(words):
        t = table(mc, addr)
        if any(v is None for v in t.values()):
            rows.append({'routine': r, 'address': addr, 'error': 'did not return'})
            continue
        size = (words[r + 1] - addr) if r + 1 < 48 and words[r + 1] > addr else None
        sig = tuple(t[k][0] for k in sorted(t))
        sigs.setdefault(sig, []).append(r)
        rows.append({'routine': r, 'address': addr, 'bytes': size, 'junction_slots': uses[r], 'depends_on': dependency(t),
                     'outputs_by_dir': outputs_by_dir(t), 'clobbers_xy': any((v[1], v[2]) != (k[1], k[2]) for k, v in t.items())})
    json.dump({'rows': rows, 'identical_groups': [v for v in sigs.values() if len(v) > 1]}, open('scratch/ghost_routines.json', 'w'))
    print('routines run:', len(rows), '| ran to RTS:', sum(1 for r in rows if 'error' not in r))
    classes = {}
    for r in rows:
        classes.setdefault(r.get('depends_on', 'error'), []).append(r['routine'])
    for k, v in sorted(classes.items()):
        print('  depends on %-10s: %2d routines %s' % (k, len(v), v))
    print('routines with identical answers on the whole grid:', [v for v in sigs.values() if len(v) > 1])
    print('routines that change X or Y:', [r['routine'] for r in rows if r.get('clobbers_xy')])


if __name__ == '__main__':
    main()
