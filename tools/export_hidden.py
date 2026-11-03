"""TASK-033-4e2: packs scratch/hidden_t*.json (tools/capture_hidden.py) into scratch/hidden.bin for tests/test_flow.cpp.
Format: 'HIDN', u32 count, per run: u32 number of keys n, n bytes (Apple key codes, bit 7 set), then n + 1 waits: u32 cycle of the key wait (the RDKEY/GETLN call) counted from $6FAE
(the Space was taken at 101,243 after T0 in every run), u8 rank ($54E1), u8 $55, 1024 bytes text page 2."""
import glob, json, struct

SPACE_TAKEN = 101243
KEYS = {'t1': [0x16, 0x03, 'C', 0x0D, '2', 0x0D], 't2': [0x16, 0x03, '7', 0x0D, 0x0D], 't3': [0x16, 0x03, 'A', 0x0D, '2', '1', 0x0D], 't4': [0x16, 0x03, 'A', 0x0D, '2', '2', 0x0D],
        't5': [0x16, 0x03, 'A', 0x0D, '0', 0x0D], 't6': [0x16, 0x03, 'A', 0x0D, 0x0D], 't7': [0x16, 0x03, 'A', 0x0D, 'A', 'B', 0x0D], 't8': [0x16, 0x03, 'C', 0x0D, '3', 0x0D],
        't9': [0x16, 0x03, 'C', 0x0D, '1', 0x0D], 't10': [0x16, 'x', '5'], 't11': [0x16, 0x03, '1', '2', 0x08, '3', 0x0D, '5', 0x0D], 't12': [0x16, 0x03, '9', 0x18, '4', 0x0D, 0x0D],
        't13': [0x16, 0x03, 'X', 0x0D, 'z', 0x0D]}
out = open('scratch/hidden.bin', 'wb')
names = sorted(KEYS, key=lambda s: int(s[1:]))
out.write(b'HIDN' + struct.pack('<I', len(names)))
for n in names:
    w = json.load(open('scratch/hidden_%s.json' % n))
    keys = [(ord(k) if isinstance(k, str) else k) | 0x80 for k in KEYS[n]]
    assert len(w) == len(keys) + 1, (n, len(w), len(keys))
    out.write(struct.pack('<I', len(keys)) + bytes(keys))
    for x in w:
        out.write(struct.pack('<IBB', x['cycle'] - SPACE_TAKEN, x['rank'], x['m55']) + bytes.fromhex(x['page']))
print(len(names), 'runs')
