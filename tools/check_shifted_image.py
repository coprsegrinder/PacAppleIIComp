"""TASK-027b3: light check of the software-shifted image routine $7E1F on the oracle (development tool; original data from scratch/).
Hooks only the entry of $7E1F (reached by JSR) and its return, runs the attract demo (the ghost-eaten score image is drawn there) and, for every call,
compares the page after the call with an independent Python emulation of the routine (rotate sequence $7E4F, then the OR / OR-clear blit).
Prints one line per call and a summary; writes the checked calls to scratch/hires_vectors9.bin (type-9 vectors, local only). Stops after 3 minutes. Usage: python -I tools/check_shifted_image.py"""
import os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import do_step
from cov_close import factory

START = time.monotonic()


def row(y):
    return (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


def shift_buf(buf, k):
    buf = bytearray(buf)
    for _ in range(k):
        for r in range(9, -1, -1):
            c = [0]

            def asl(v):
                c[0] = v >> 7
                return (v << 1) & 255

            def rol(v):
                o = c[0]
                c[0] = v >> 7
                return ((v << 1) | o) & 255

            def ror(v):
                o = c[0]
                c[0] = v & 1
                return (v >> 1) | (o << 7)
            b0, b1, b2 = buf[r * 3:r * 3 + 3]
            b0 = asl(b0); b0 = asl(b0); b1 = rol(b1); b1 = asl(b1); b2 = rol(b2); b1 = ror(b1); b0 = ror(b0)
            buf[r * 3:r * 3 + 3] = bytes([b0, b1, b2])
    return buf


def emulate(before, img, x, y, mode_or):
    page = bytearray(before)
    px = x - 5
    buf = shift_buf(img, px % 7)
    for r in range(10):
        for b in range(3):
            a = row(y - 4 + r) + px // 7 + b
            if a < 8192:
                v = page[a] | buf[r * 3 + b]
                page[a] = v if mode_or else v & 0x7F
    return bytes(page)


def save_pairs(pairs, path='scratch/hires_vectors9.bin'):
    """The checked calls as type-9 vectors (format of tools/capture_hires_vectors.py); local only, never committed."""
    import struct
    with open(path, 'wb') as f:
        f.write(b'HIRV' + struct.pack('<I', len(pairs)))
        for ptr, x, y, fl, before, after in pairs:
            f.write(struct.pack('<B8i', 9, ptr, x, y, fl[0], fl[1], 0, 0, 0) + before + after)


def main():
    mc = factory()
    cap = {}
    results = []
    pairs = []
    step = mc.cpu.step

    def hook():
        pc, c, m = mc.cpu.pc, mc.cpu, mc.m
        if pc == 0x7E1F and 'x' not in cap and m[mc.cpu.sp + 0x100 + 1] | m[mc.cpu.sp + 0x100 + 2] << 8 in (0x7DF1, 0x7EF1, 0x8545):   # entered by JSR from $7DEF / $7EEF / $8543
            sp = c.sp
            cap.update(x=m[0x0C] | m[0x0D] << 8, y=m[0x10], ptr=m[0x16] | m[0x17] << 8, f=(m[0x1D], m[0x1E]), ret=(((m[0x100 + ((sp + 2) & 0xFF)] << 8 | m[0x100 + ((sp + 1) & 0xFF)]) + 1) & 0xFFFF),
                       sp=(sp + 2) & 0xFF, before=bytes(m[0x2000:0x4000]), img=bytes(m[m[0x16] | m[0x17] << 8:(m[0x16] | m[0x17] << 8) + 30]))
        elif 'x' in cap and pc == cap['ret'] and c.sp == cap['sp']:
            after = bytes(m[0x2000:0x4000])
            emu = emulate(cap['before'], cap['img'], cap['x'], cap['y'], cap['f'][0] and cap['f'][1])
            nd = sum(1 for i in range(8192) if cap['before'][i] != after[i])
            ne = sum(1 for i in range(8192) if emu[i] != after[i])
            results.append((cap['ptr'], cap['x'], cap['y'], cap['f'], nd, ne))
            pairs.append((cap['ptr'], cap['x'], cap['y'], cap['f'], cap['before'], after))
            cap.clear()
        return step()
    mc.cpu.step = hook
    if len(sys.argv) > 1 and sys.argv[1] == 'cartoon3':          # the same check inside cartoon 3 (hidden menu), where the ghost images $856C / $854E are drawn
        import explore
        explore.to_players_prompt(mc)
        explore.hidden_menu(mc, 'C')
        explore.until_key(mc, 5_000_000)
        explore.send(mc, ['3', chr(13)])
        explore.until_key(mc, 400_000_000)
        out = 'scratch/hires_vectors9c.bin'
    else:
        out = 'scratch/hires_vectors9.bin'
        for st in ('key:1', 'key:cr'):
            do_step(mc, st, lambda *_: None)
        while time.monotonic() - START < 180 and len(results) < 40:
            try:
                mc.run(1_000_000)
            except NeedKey:
                mc.push_key(13)
    for r in results:
        print('ptr $%04X x %3d y %3d flags %s: oracle changed %2d bytes, emulation differs from the oracle in %d bytes' % r)
    save_pairs(pairs, out)
    bad = [r for r in results if r[5]]
    print('calls checked', len(results), '| differing', len(bad), '| elapsed %.0f s' % (time.monotonic() - START))


if __name__ == '__main__':
    main()
