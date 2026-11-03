"""TASK-033-3b: writes scratch/death_timing.bin for the C++ test (development tool; original data from scratch/): the inputs and the measured landmarks of every recorded death
that has the dot map saved. Format: 'DTH1', u32 count, then per death 7 x i32 inputs (pacX, dots, energizers, scoreDigits, highDigits, livesBefore, level) and 29 x i32 landmarks
(c1..c5, img, c6, f1..f6, s1..s6, m1..m6, rebuild, ready, last_blit, total).   Usage: python -I tools/export_deaths.py"""
import glob, json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import death_lib

NAMES = ['c1', 'c2', 'c3', 'c4', 'c5', 'img', 'c6'] + ['f%d' % k for k in range(1, 7)] + ['s%d' % k for k in range(1, 7)] + ['m%d' % k for k in range(1, 7)] + ['rebuild', 'ready', 'last_blit', 'total']


def digits(z):
    s = '%x' % (z[0x54] << 16 | z[0x53] << 8 | z[0x52])
    return len(s.lstrip('0')) or 1


recs = []
for pat in ('scratch/deaths_forced2_7*.json', 'scratch/deaths_forced_8*.json'):
    for f in sorted(glob.glob(pat)):
        for d in json.load(open(f))['deaths']:
            if 'events' not in d or 'map' not in d:
                continue
            r = death_lib.landmarks(d)
            m = bytes.fromhex(d['map'])
            cells = [m[c * 32 + rw] for c in range(27) for rw in range(1, 31)]
            dots = sum(1 for v in cells if v and v & 1)
            en = sum(1 for v in cells if v and not v & 1)
            z = r['zp']
            inp = [r['pac'][0], dots, en, digits(z), digits(z), z[0x58] & 0x7F, z[0x55]]
            recs.append(inp + [r['L'][n] for n in NAMES])
with open('scratch/death_timing.bin', 'wb') as out:
    out.write(b'DTH1' + struct.pack('<I', len(recs)))
    for rec in recs:
        out.write(struct.pack('<%di' % len(rec), *rec))
print(len(recs), 'deaths written, record size', 4 * len(recs[0]))
