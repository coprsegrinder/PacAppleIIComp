"""TASK-027b3: replace the type-9 records of scratch/hires_vectors.bin by the verified ones in scratch/hires_vectors9.bin (demo) and scratch/hires_vectors9c.bin (cartoon 3) (local files, never committed).
Usage: python -I tools/merge_type9.py"""
import struct

REC = 1 + 32 + 2 * 8192


def records(path):
    d = open(path, 'rb').read()
    n = struct.unpack('<I', d[4:8])[0]
    return [d[8 + i * REC: 8 + (i + 1) * REC] for i in range(n)]


old = records('scratch/hires_vectors.bin')
keep = [r for r in old if r[0] != 9]
import os
new = records('scratch/hires_vectors9.bin')
if os.path.exists('scratch/hires_vectors9c.bin'):
    new += records('scratch/hires_vectors9c.bin')
with open('scratch/hires_vectors.bin', 'wb') as f:
    f.write(b'HIRV' + struct.pack('<I', len(keep) + len(new)) + b''.join(keep) + b''.join(new))
print('kept', len(keep), 'old records, dropped', len(old) - len(keep), 'old type-9 records, added', len(new), 'verified type-9 records')
