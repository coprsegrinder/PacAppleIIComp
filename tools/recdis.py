"""Recursive-descent 6502 disassembly: follow JSR/JMP/branches from entry points.
Usage: python -I tools/recdis.py <body.bin> <origin-hex> <entry-hex>[,<entry-hex>...] <out-prefix>
Writes <out-prefix>.lst (listing of reached code) and <out-prefix>.json (summary). Reached = proven code
(assuming no self-modifying code); everything else is UNKNOWN, not necessarily data."""
import sys, os, json
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import OPS, MODES, disassemble

def analyse(body, origin, entries):
    end = origin + len(body)
    inside = lambda a: origin <= a < end
    code, work, labels = {}, list(entries), set(entries)
    indirect, outside, overlaps = [], set(), []
    while work:
        a = work.pop()
        while inside(a) and a not in code:
            op = body[a - origin]
            if op not in OPS: break                       # undocumented/invalid opcode => not code, stop this path
            mn, mode = OPS[op]; size = MODES[mode][0] + 1
            if a + size > end: break
            raw = body[a - origin:a - origin + size]
            code[a] = size
            if mode == 'rel':
                t = (a + 2 + (raw[1] - 256 if raw[1] > 127 else raw[1])) & 0xFFFF
                labels.add(t); work.append(t)
            elif mn in ('JSR', 'JMP') and mode == 'abs':
                t = raw[1] | raw[2] << 8
                if inside(t): labels.add(t); work.append(t)
                else: outside.add(t)
            elif mn == 'JMP': indirect.append(a)
            if mn in ('JMP', 'RTS', 'RTI', 'BRK'): break  # control does not fall through
            a += size
    # detect instructions whose bytes overlap another instruction (sign of a wrong path or data-in-code tricks)
    cover = {}
    for a, s in sorted(code.items()):
        for b in range(a, a + s):
            if b in cover: overlaps.append((cover[b], a))
            cover[b] = a
    return code, labels, indirect, outside, overlaps

if __name__ == '__main__':
    body = open(sys.argv[1], 'rb').read(); origin = int(sys.argv[2], 16)
    entries = [int(x, 16) for x in sys.argv[3].split(',')]; prefix = sys.argv[4]
    code, labels, indirect, outside, overlaps = analyse(body, origin, entries)
    covered = sum(code.values())
    with open(prefix + '.lst', 'w') as f:
        prev_end = None
        for a, s in sorted(code.items()):
            if prev_end is not None and a != prev_end: f.write('\n')
            ins = disassemble(body[a - origin:a - origin + s], a)[0]
            f.write(f"{'L%04X:' % a if a in labels else '':<7}{a:04X}  {ins[1].hex(' '):<9} {ins[2]}\n"); prev_end = a + s
    json.dump(dict(origin=origin, size=len(body), code_bytes=covered, instructions=len(code), labels=len(labels),
                   indirect_jumps=[f"{a:04X}" for a in indirect], outside_targets=sorted(f"{a:04X}" for a in outside),
                   overlaps=[(f"{a:04X}", f"{b:04X}") for a, b in overlaps]), open(prefix + '.json', 'w'), indent=1)
    print(f"reached {len(code)} instructions, {covered} of {len(body)} bytes ({100*covered/len(body):.1f}%), {len(labels)} labels")
    print("indirect jumps:", [f"{a:04X}" for a in indirect]); print("overlaps:", len(overlaps))
    print("distinct outside targets:", len(outside))
