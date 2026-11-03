"""TASK-033-4f: packs scratch/boot_b*.json (tools/capture_boot.py) into scratch/boot.bin for tests/test_boot.cpp.
Format: 'BOOT', u32 count, per run: u32 number of keys n, n bytes (Apple key codes, bit 7 set), then n + 1 waits: u32 cycle of the key wait (from $4027), u8 $F5, 4 bytes ($6FBA, $6FB6,
$6FB2, $6FBE), u32 T0 (the cycle of $52BB at the last wait, else 0), 1024 bytes text page 2, 256 bytes zero page."""
import glob, json, struct
KEYS = {'b1': '1N', 'b2': '2', 'b3': '1CJLIM', 'b4': '1CJJLLJLIIM', 'b5': '3x1z', 'b6': '1c', 'b7': ['1', 'C', 0x0D, 0x20, 'A', 'B'], 'b8': ['1', 'C', 0x1B, 'a', 'b', 'c']}
out = open('scratch/boot.bin', 'wb')
names = sorted(KEYS, key=lambda s: int(s[1:]))
out.write(b'BOOT' + struct.pack('<I', len(names)))
for n in names:
    w = json.load(open('scratch/boot_%s.json' % n))
    keys = [(ord(k) if isinstance(k, str) else k) | 0x80 for k in KEYS[n]]
    assert len(w) == len(keys) + 1, (n, len(w), len(keys))
    out.write(struct.pack('<I', len(keys)) + bytes(keys))
    for i, x in enumerate(w):
        out.write(struct.pack('<IB4BI', x['cycle'], x['f5'], *x['keys'], x.get('t0') or 0) + bytes.fromhex(x['page']) + bytes.fromhex(x['zp']))
print(len(names), 'runs')
