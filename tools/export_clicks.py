"""TASK-032-2: export the original's click times for all 12 sounds, as measured on the oracle by tools/sound_model.py (development tool; original data from scratch/).
File scratch/sound_clicks.bin (LOCAL ONLY, derived from original data, git-ignored): b'CLK1', u32 number of sounds, then per sound in the order of
tools/sound_model.SOUNDS: u32 number of clicks, then u32 per click = cycles since the first click.
Usage: python -I tools/export_clicks.py      (about 1 minute)"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import sound_model as S
from play import make_machine

mc = make_machine()
out = b'CLK1' + struct.pack('<I', len(S.SOUNDS))
for name, how, eng, addr in S.SOUNDS:
    got = S.oracle_clicks(mc, eng, addr)
    out += struct.pack('<I', len(got)) + b''.join(struct.pack('<I', t) for t in got)
    gaps = sorted(set(b - a for a, b in zip(got, got[1:])))
    print('%-16s %4d clicks, last at %8d cycles, distinct gaps %d (first few %s)' % (name, len(got), got[-1], len(gaps), gaps[:4]))
open('scratch/sound_clicks.bin', 'wb').write(out)
print('written', len(out), 'bytes')
