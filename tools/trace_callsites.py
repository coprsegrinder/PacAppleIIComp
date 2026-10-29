"""Who draws what: record every call into the draw entry points and group by caller (development tool; uses original data from scratch/).
Entry points: $6A20 (draw dispatcher: A/Y = table base) and $55A6 (OR blitter, base already patched into $55B2/$55B3).
For each (entry, caller) it records the bases used, width x rows, and the screen rows/columns drawn. Usage: python -I tools/trace_callsites.py [max_instr]
Writes scratch/callsites.json and prints a summary.
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey

ENTRIES = {0x6A20: 'dispatch', 0x55A6: 'orblit'}


def install(mc, entries=ENTRIES):
    """Hook cpu.step; at an entry address log the caller (return address on the stack) and the drawing parameters."""
    sites = {}
    step = mc.cpu.step
    last = [0]                                   # address of the previously executed instruction

    def hooked():
        pc = mc.cpu.pc
        prev, last[0] = last[0], pc
        if pc in entries and not (pc == 0x55A6 and prev == 0x55C4):    # $55C4 = loop-back BNE: not a new blit
            sp = mc.cpu.sp
            caller = ((mc.m[0x100 + ((sp + 2) & 0xFF)] << 8 | mc.m[0x100 + ((sp + 1) & 0xFF)]) - 2) & 0xFFFF   # address of the JSR
            if pc == 0x6A20:
                base = mc.cpu.a | mc.cpu.y << 8
            else:
                base = mc.m[0x55B2] | mc.m[0x55B3] << 8
            s = sites.setdefault((pc, caller), {'bases': {}, 'rows': set(), 'cols': set(), 'n': 0})
            s['n'] += 1
            key = (base, mc.m[0x1A], mc.m[0x18])
            s['bases'][key] = s['bases'].get(key, 0) + 1
            s['rows'].add(mc.m[0x10])
            s['cols'].add(mc.m[0x04])
        return step()
    mc.cpu.step = hooked
    return sites


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 80_000_000
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    mc.run(10_000_000, stop_pc=0x4027)
    sites = install(mc)
    mc.push_key('1')
    try:
        mc.run(10_000_000)
    except NeedKey:
        pass
    mc.push_key('N')
    mc.run(limit)
    out = []
    for (entry, caller), s in sorted(sites.items()):
        out.append({'entry': entry, 'caller': caller, 'calls': s['n'], 'rows': [min(s['rows']), max(s['rows'])],
                    'cols': [min(s['cols']), max(s['cols'])],
                    'bases': [[b, w, r, n] for (b, w, r), n in sorted(s['bases'].items())]})
    json.dump(out, open('scratch/callsites.json', 'w'))
    for o in out:
        shapes = sorted({(w, r) for _, w, r, _ in o['bases']})
        bs = sorted({b for b, _, _, _ in o['bases']})
        print('%04X<-%04X calls=%d rows %d-%d cols %d-%d | %d base(s) %s%s | sizes %s' % (
            o['entry'], o['caller'], o['calls'], *o['rows'], *o['cols'], len(bs),
            ' '.join('%04X' % b for b in bs[:6]), '...' if len(bs) > 6 else '', shapes[:4]))


if __name__ == '__main__':
    main()
