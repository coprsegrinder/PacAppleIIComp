"""TASK-034-DECIDE: vectors that make every conditional branch of the 48 ghost decision routines go BOTH ways on the oracle (development tool; original data from scratch/).
tools/record_decide_vectors.py draws 2,000 random inputs per routine; the line-coverage run (docs/CODE_COVERAGE_PORT.md) showed that a few branch directions were never taken by them (for example
the Y = $6B test of the routine at $9627, which a random Y hits one time in 256). Here the inputs are searched: values on either side of every constant that a routine compares X or Y with, values
equal to the ghost's own coordinates and one off them, all directions. A vector is kept when it takes a branch direction that no earlier vector of its routine took. At the end every branch direction
that was never taken is listed: those are the pairs such as `BEQ x / BNE y` whose second branch cannot fail (checked below by a complete search over the classes of the inputs).
Writes scratch/decide_vectors_edges.bin in the format of tools/record_decide_vectors.py (pass it to the tests as --decide scratch/decide_vectors.bin,scratch/decide_vectors_edges.bin).
Usage: python -I tools/record_decide_edges.py"""
import itertools, os, random, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import play
from dis6502 import disassemble

mc = play.make_machine()
body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
tab = [body[0x8DBE - 0x4000 + 2 * i] | body[0x8DBE - 0x4000 + 2 * i + 1] << 8 for i in range(48)]
entries = sorted(set(tab))
ends = {e: (entries[i + 1] if i + 1 < len(entries) else 0x96E8) for i, e in enumerate(entries)}   # (the last routine ends at $96E8; a table of words follows)
STOP = 0x0300
mc.m[STOP] = 0xEA
cpu = mc.cpu
BRANCHES = {'BCC', 'BCS', 'BEQ', 'BNE', 'BPL', 'BMI', 'BVC', 'BVS'}


def code_info(entry):
    """Branch sites and the immediate constants compared with X, Y or A in the routine's bytes."""
    sites, consts = [], {'X': set(), 'Y': set(), 'A': set()}
    for addr, raw, text in disassemble(body, 0x4000, entry - 0x4000, ends[entry] - entry):
        mn = text.split()[0]
        if mn in BRANCHES:
            sites.append(addr)
        if len(raw) == 2 and text.split()[1].startswith('#$'):
            v = int(text.split()[1][2:], 16)
            if mn == 'CPX':
                consts['X'].add(v)
            elif mn == 'CPY':
                consts['Y'].add(v)
            elif mn == 'CMP':
                consts['A'].add(v)
    return sites, consts


COMPLEMENT = {'BEQ': 'BNE', 'BNE': 'BEQ', 'BCC': 'BCS', 'BCS': 'BCC', 'BMI': 'BPL', 'BPL': 'BMI', 'BVC': 'BVS', 'BVS': 'BVC'}


def explained(entry, missing):
    """Branch directions that cannot happen by construction: the instruction before the branch is a branch whose fall-through fixes the flag (BEQ x / BNE y: the second always
    branches; BCS x / BCS y: the second never does) and nothing else jumps to the second branch. Returns (explained, unexplained)."""
    ins = {}
    targets = set()
    order = []
    for addr, raw, text in disassemble(body, 0x4000, entry - 0x4000, ends[entry] - entry):
        parts = text.split()
        ins[addr] = (parts[0], len(raw))
        order.append(addr)
        if parts[0] in BRANCHES:
            targets.add((addr + 2 + (raw[1] - 256 if raw[1] > 127 else raw[1])) & 0xFFFF)
        elif parts[0] in ('JMP', 'JSR') and len(raw) == 3:
            targets.add(raw[1] | raw[2] << 8)
    ok, bad = [], []
    for s, taken in missing:
        prev = None
        for a in order:
            if a + ins[a][1] == s:
                prev = a
        pm = ins[prev][0] if prev is not None else None
        sm = ins[s][0]
        if s not in targets and pm in BRANCHES and ((not taken and COMPLEMENT[pm] == sm) or (taken and pm == sm)):
            ok.append((s, taken))
        else:
            bad.append((s, taken))
    return ok, bad


def around(vals):
    out = {0, 1, 2, 3, 254, 255}
    for v in vals:
        out.update(((v - 1) & 255, v & 255, (v + 1) & 255))
    return sorted(out)


def run(entry, a, x, y, gx, gy, gdir, pdir, ptun, sites):
    mc.m[0x70], mc.m[0x71], mc.m[0x72], mc.m[0x62], mc.m[0x69] = gx, gy, gdir, pdir, ptun
    cpu.a, cpu.x, cpu.y = a, x, y
    cpu.sp = 0xF0
    cpu.pc = entry
    cpu.push((STOP - 1) >> 8)
    cpu.push((STOP - 1) & 255)
    edges, n = set(), 0
    while cpu.pc != STOP and n < 2000:
        pc0 = cpu.pc
        cpu.step()
        if pc0 in sites:
            edges.add((pc0, cpu.pc != pc0 + 2))
        n += 1
    return edges, 1 if cpu.pc == STOP else 0


def prove(entry, bad):
    """The branch directions left over, checked by a search over EVERY value of X (0..255) with Y at every constant of the routine and its neighbours, all A and Pac-Man directions and four positions
    of the ghost. Returns the directions that still never happened (all of them, if the argument in docs/ghost decision routines holds)."""
    sites, consts = code_info(entry)
    site_set = set(sites)
    ys = around(consts['Y'])
    left = set(bad)
    for gx, gy, a, pd, x, y in itertools.product((0x20, 0x50, 0x90, 0xB0), (0x40, 0x90), range(4), range(4), range(256), ys):
        edges, _ = run(entry, a, x, y, gx, gy, 0, pd, 0, site_set)
        left -= edges
        if not left:
            break
    return sorted(left)


def main():
    rng = random.Random(3030)
    recs, left_over = [], []
    for entry in tab:
        sites, consts = code_info(entry)
        site_set = set(sites)
        want = {(s, t) for s in sites for t in (True, False)}
        ys, xs = around(consts['Y']), around(consts['X'])
        seen = set()
        kept = 0
        tries = 0

        def attempt(a, x, y, gx, gy, gdir, pdir, ptun):
            nonlocal kept
            edges, ok = run(entry, a, x, y, gx, gy, gdir, pdir, ptun, site_set)
            if edges - seen:
                seen.update(edges)
                kept += 1
                recs.append(struct.pack('<HBBBBBBBB', entry, a, x, y, gx, gy, gdir, pdir, ptun) + bytes([cpu.a, mc.m[0x70], mc.m[0x71], mc.m[0x72], ok]))

        for tries in range(40000):
            if seen >= want:
                break
            gx = rng.choice(xs) if rng.random() < 0.7 else rng.randrange(256)
            gy = rng.choice(ys) if rng.random() < 0.7 else rng.randrange(256)
            x = (gx + rng.choice([-1, 0, 0, 1])) & 255 if rng.random() < 0.4 else (rng.choice(xs) if rng.random() < 0.7 else rng.randrange(256))
            y = (gy + rng.choice([-1, 0, 0, 1])) & 255 if rng.random() < 0.4 else (rng.choice(ys) if rng.random() < 0.7 else rng.randrange(256))
            attempt(rng.randrange(4), x, y, gx, gy, rng.randrange(4), rng.randrange(4), rng.randrange(2))
        missing = sorted(want - seen)
        if missing:
            # a complete search over the classes of the inputs: X and Y each below / equal / above the ghost's coordinate and around every constant, all directions
            gx0, gy0 = 0x50, 0x90
            xs2 = sorted(set(around(consts['X']) + [gx0 - 1, gx0, gx0 + 1]))
            ys2 = sorted(set(around(consts['Y']) + [gy0 - 1, gy0, gy0 + 1]))
            for a, x, y, gd, pd, pt in itertools.product(range(4), xs2, ys2, range(4), range(4), range(2)):
                attempt(a, x, y, gx0, gy0, gd, pd, pt)
            missing = sorted(want - seen)
        ok, bad = explained(entry, missing)
        left_over.append((entry, bad, kept, tries))
        print('routine %04X: %d branch directions, %d vectors kept, impossible by construction: %d, UNEXPLAINED: %s' % (entry, len(want), kept, len(ok), ', '.join('%04X%s' % (s, 'T' if t else 'N') for s, t in bad) or 'none'), flush=True)
    with open('scratch/decide_vectors_edges.bin', 'wb') as f:
        f.write(b'DECV' + struct.pack('<I', len(recs)))
        for r in recs:
            f.write(r)
    if '--prove' in sys.argv:
        for e, m, _, _ in left_over:
            if m:
                print('routine %04X: directions never taken in the complete search over X: %s (given: %s)' % (e, ', '.join('%04X%s' % (s_, 'T' if t_ else 'N') for s_, t_ in prove(e, m)), ', '.join('%04X%s' % (s_, 'T' if t_ else 'N') for s_, t_ in m)), flush=True)
    print('vectors', len(recs), 'routines with an unexplained branch direction:', [('%04X' % e, [('%04X%s' % (s, 'T' if t else 'N')) for s, t in m]) for e, m, _, _ in left_over if m])


if __name__ == '__main__':
    main()
