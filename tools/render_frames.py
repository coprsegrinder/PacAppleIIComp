"""Capture every shape drawn by the two blitters ($55A6 and $6A3F) in the usual scenario and render them to a PNG contact sheet.
Development tool: reads original data from scratch/ and writes images to scratch/ (git-ignored; never commit them).
Usage: python -I tools/render_frames.py [max_instr]   -> scratch/frames.png, scratch/frames.txt
"""
import os, struct, sys, zlib
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey

# (instruction address that loads the width, address of the table-read instruction)
BLITTERS = {0x55AD: 0x55B1, 0x6A46: 0x6A4A}
VIOLET, GREEN, BLUE, ORANGE, WHITE = (255, 68, 253), (20, 245, 60), (20, 207, 253), (255, 106, 60), (255, 255, 255)


def track(mc):
    """Hook the CPU step: at each blitter row start log (reader, base, x, width, row, col); merge rows into blits."""
    blits, state = [], {}
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        r = BLITTERS.get(pc)
        if r is not None:
            base = mc.m[r + 1] | mc.m[r + 2] << 8
            x, w, row, col = mc.m[3], mc.m[0x1A], mc.m[0x10], mc.m[4]
            last = state.get(r)
            if last and last['base'] == base and last['w'] == w and x == last['x'] + w and row == last['row'] + 1:
                last['rows'] += 1
                last['x'] += w
                last['row'] += 1
            else:
                last = {'r': r, 'base': base, 'x0': x, 'x': x, 'w': w, 'row': row, 'row0': row, 'col': col, 'rows': 1, 'n': 1}
                state[r] = last
                blits.append(last)
        return step()
    mc.cpu.step = hooked
    return blits


def pixels(data, w, rows, col):
    """Decode Apple II hi-res bytes (bit0 = leftmost pixel, bit7 = palette) to rows of RGB tuples (None = black)."""
    img = []
    for r in range(rows):
        line = data[r * w:(r + 1) * w]
        on = [(b >> i) & 1 for b in line for i in range(7)]
        pal = [(b >> 7) & 1 for b in line for _ in range(7)]
        out = []
        for i, v in enumerate(on):
            if not v:
                out.append((0, 0, 0))
            elif (i > 0 and on[i - 1]) or (i + 1 < len(on) and on[i + 1]):
                out.append(WHITE)
            else:
                even = (col * 7 + i) % 2 == 0
                out.append((VIOLET if even else GREEN) if not pal[i] else (BLUE if even else ORANGE))
        img.append(out)
    return img


def write_png(path, rows):
    h, w = len(rows), len(rows[0])
    raw = b''.join(b'\x00' + bytes(c for px in row for c in px) for row in rows)

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def sheet(frames, body, scale=3, width=1100, pad=6):
    cells = []
    for f in frames:
        o = f['base'] + f['x0'] - 0x4000
        data = body[o:o + f['w'] * f['rows']]
        cells.append(pixels(data, f['w'], f['rows'], f['col']))
    out, x, y, rowh, placed = [], pad, pad, 0, []
    for i, img in enumerate(cells):
        cw, ch = len(img[0]) * scale, len(img) * scale
        if x + cw + pad > width:
            x, y, rowh = pad, y + rowh + pad, 0
        placed.append((x, y, cw, ch, img))
        x += cw + pad
        rowh = max(rowh, ch)
    H = y + rowh + pad
    canvas = [[(40, 40, 40)] * width for _ in range(H)]
    for x, y, cw, ch, img in placed:
        for ry, row in enumerate(img):
            for rx, px in enumerate(row):
                for dy in range(scale):
                    for dx in range(scale):
                        canvas[y + ry * scale + dy][x + rx * scale + dx] = px
    return canvas, [(p[0], p[1]) for p in placed]


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 80_000_000
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    mc.run(10_000_000, stop_pc=0x4027)
    blits = track(mc)
    mc.push_key('1')
    try:
        mc.run(10_000_000)
    except NeedKey:
        pass
    mc.push_key('N')
    mc.run(limit)
    uniq = {}
    for b in blits:
        k = (b['r'], b['base'], b['x0'], b['w'], b['rows'])
        if k in uniq:
            uniq[k]['n'] += 1
        else:
            uniq[k] = dict(b)
    frames = sorted(uniq.values(), key=lambda f: (f['base'] + f['x0'], f['r']))
    frames = [f for f in frames if 0x4000 <= f['base'] + f['x0'] < 0xAA00 and f['w'] * f['rows'] <= 400]
    canvas, pos = sheet(frames, body)
    write_png('scratch/frames.png', canvas)
    with open('scratch/frames.txt', 'w') as fh:
        for i, (f, p) in enumerate(zip(frames, pos)):
            fh.write('%3d sheet@%s reader=%04X data=%04X width=%d rows=%d count=%d first_row=%d col=%d\n' % (
                i, p, f['r'], f['base'] + f['x0'], f['w'], f['rows'], f['n'], f['row0'], f['col']))
    print(len(blits), 'blits;', len(frames), 'distinct frames; sheet', len(canvas[0]), 'x', len(canvas))


if __name__ == '__main__':
    main()
