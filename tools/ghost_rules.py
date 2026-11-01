"""Complete decision tables of the 48 ghost direction routines (TASK-030b; development tool, runs the original code on the Python oracle).
Static scan (see docs/GHOST_RULES.md): the routines only read A (incoming direction), X/Y (target), the ghost position $70/$71 and
three more bytes: $62 (Pac-Man's direction), $69 (Pac-Man tunnel flag), $72 (ghost's direction). Every comparison is against an
immediate constant or against the ghost coordinate (or an abs-difference threshold of $20), so for a fixed ghost position the answer
only changes at a finite set of breakpoints per axis. This tool builds, per routine and per real ghost position, the answer for
every class of X and Y (each breakpoint, and one value inside each gap), then checks the tables against the oracle on random
full-range inputs.   Usage: python -I tools/ghost_rules.py [samples-per-routine]   -> writes scratch/ghost_rules.json
"""
import json, os, random, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine
from dis6502 import disassemble
import ghost_routines
from ghost_routines import run_routine
from junction_graph import parse_records

ORIGIN, WORDS, INDEX = 0x4000, 0x8DBE, 0x8CBE
READS_OK = {0x62, 0x69, 0x70, 0x71, 0x72}      # zero-page bytes the routines may read
END = 0x96E8                                   # end of the last routine


def routine_words(body):
    return [body[WORDS - ORIGIN + 2 * i] | body[WORDS - ORIGIN + 2 * i + 1] << 8 for i in range(48)]


def closure_scan(body, words, r):
    """Disassemble routine r and every routine it JMP/JSRs into; returns (cx, cy, zp_reads, abs_diff, unexpected_ops)."""
    starts = sorted(set(words))
    seen, todo = set(), [words[r]]
    cx, cy, zp, absdiff, bad = set(), set(), set(), False, []
    while todo:
        a = todo.pop()
        if a in seen:
            continue
        seen.add(a)
        nxt = min([s for s in starts if s > a] + [END])
        for ad, raw, txt in disassemble(body, ORIGIN, a - ORIGIN, nxt - a):
            m = re.match(r'(CPX|CPY) #\$([0-9A-F]{2})', txt)
            if m:
                (cx if m.group(1) == 'CPX' else cy).add(int(m.group(2), 16))
            m = re.match(r'(LDA|SBC|STA) \$([0-9A-F]{2})$', txt)
            if m:
                zp.add(int(m.group(2), 16))
            if txt.startswith('SBC $7'):
                absdiff = True
            m = re.match(r'(JMP|JSR) \$([0-9A-F]{4})', txt)
            if m:
                todo.append(int(m.group(2), 16))
            if re.match(r'(ADC|AND|ORA|ASL|LSR|ROL|ROR|INC|DEC|LDX|LDY|STX|STY|BIT)', txt):
                bad.append(txt)
    return cx, cy, zp, absdiff, bad


def breakpoints(consts, g, absdiff):
    b = set(consts) | {g}
    if absdiff:
        for k in (0x1F, 0x20, 0x21):
            b |= {g - k, g + k}
    return sorted(v for v in b if 0 <= v <= 255)


def class_reps(b):
    """One representative per equivalence class: each breakpoint itself and the first value of every gap between breakpoints."""
    reps, lo = set(), 0
    for e in b + [256]:
        if lo < e:
            reps.add(lo)
        if e < 256:
            reps.add(e)
        lo = e + 1
    return sorted(reps)


def class_of(v, b):
    """Map a raw value to its class representative (breakpoint itself, or the first value of its gap)."""
    if v in b:
        return v
    return max([x for x in b if x < v] + [-1]) + 1


def evaluate(mc, addr, a, x, y, gx, gy, p62, p69, p72):
    mc.m[0x62], mc.m[0x69], mc.m[0x72] = p62, p69, p72
    ghost_routines.GHOST = (gx, gy)
    res = run_routine(mc, addr, a, x, y)
    return None if res is None else res[0]


def build_tables(nsamples=300, seed=30, only=None):
    """Returns (info, summary). info[r] describes routine r; info[r]['table_rows']['gx,gy'] has the class tables."""
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    words = routine_words(body)
    index = body[INDEX - ORIGIN:INDEX - ORIGIN + 256]
    recs = parse_records(body)
    mc = Machine()
    mc.load(ORIGIN, body)
    rng = random.Random(seed)
    info = {}
    summary = {'checked_points': 0, 'mismatches': 0, 'table_cells': 0, 'no_return': 0}
    for r, addr in enumerate(words):
        if only is not None and r not in only:
            continue
        positions = sorted({(recs[s // 4]['x'], recs[s // 4]['y']) for s in range(256) if index[s] == r})
        cx, cy, zp, absdiff, bad = closure_scan(body, words, r)
        reads = {z for z in zp if z in READS_OK}
        d = {'address': addr, 'positions': positions, 'cx': sorted(cx), 'cy': sorted(cy), 'absdiff': absdiff, 'zp_reads': sorted(reads),
             'unknown_zp': sorted(z for z in zp if z not in READS_OK), 'unexpected_ops': bad}
        p62s = range(4) if 0x62 in reads else (0,)
        p69s = range(2) if 0x69 in reads else (0,)
        p72s = range(4) if 0x72 in reads else (0,)
        tabs = {}
        for (gx, gy) in positions:
            bx, by = breakpoints(cx, gx, absdiff), breakpoints(cy, gy, absdiff)
            rx, ry = class_reps(bx), class_reps(by)
            t = {}
            for a in range(4):
                for p62 in p62s:
                    for p69 in p69s:
                        for p72 in p72s:
                            for x in rx:
                                for y in ry:
                                    v = evaluate(mc, addr, a, x, y, gx, gy, p62, p69, p72)
                                    summary['no_return'] += v is None
                                    t[(a, p62, p69, p72, x, y)] = v
            summary['table_cells'] += len(t)
            tabs[(gx, gy)] = (bx, by, t)
            for _ in range(nsamples // max(1, len(positions)) + 1):
                a, x, y = rng.randrange(4), rng.randrange(256), rng.randrange(256)
                q62, q69, q72 = rng.choice(list(p62s)), rng.choice(list(p69s)), rng.choice(list(p72s))
                got = evaluate(mc, addr, a, x, y, gx, gy, q62, q69, q72)
                want = t[(a, q62, q69, q72, class_of(x, bx), class_of(y, by))]
                summary['checked_points'] += 1
                summary['mismatches'] += got != want
        dep = set()
        for (bx, by, t) in tabs.values():
            for k, v in t.items():
                for i, name in ((0, 'dir'), (1, '$62'), (2, '$69'), (3, '$72'), (4, 'x'), (5, 'y')):
                    if name in dep:
                        continue
                    for k2, v2 in t.items():
                        if k2[i] != k[i] and all(k2[j] == k[j] for j in range(6) if j != i) and v2 != v:
                            dep.add(name)
                            break
        d['depends_on'] = sorted(dep)
        d['answers'] = sorted({v for (_, _, t) in tabs.values() for v in t.values() if v is not None})
        d['cells'] = sum(len(t) for (_, _, t) in tabs.values())
        d['table_rows'] = {'%d,%d' % p: {'bx': bx, 'by': by, 'cells': {','.join(map(str, k)): v for k, v in t.items()}} for p, (bx, by, t) in tabs.items()}
        info[r] = d
    return info, summary


def main():
    nsamples = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    info, summary = build_tables(nsamples)
    json.dump({'routines': info, 'summary': summary}, open('scratch/ghost_rules.json', 'w'))
    print('summary', summary)
    for r, d in sorted(info.items()):
        print('r%02d %04X pos=%2d cx=%d cy=%d absdiff=%d reads=%s unknown_zp=%s deps=%s answers=%s cells=%d%s' % (
            r, d['address'], len(d['positions']), len(d['cx']), len(d['cy']), d['absdiff'], d['zp_reads'], d['unknown_zp'], d['depends_on'],
            d['answers'], d['cells'], ' BAD:' + str(d['unexpected_ops']) if d['unexpected_ops'] else ''))


if __name__ == '__main__':
    main()
