"""TASK-031f: contact sheet of the original's sprite tables, to see what each frame table depicts (development tool; original data from scratch/;
the output PNG goes to scratch/, which is git-ignored, and must never be committed).
Uses the standard Apple II colour model (docs/GRAPHICS.md section 9). Shift-0 copy of each image (3 x 10 bytes = 21 x 10 pixels unless stated).
Usage: python -I tools/render_sheet.py [out.png]"""
import sys
import numpy as np
from PIL import Image, ImageDraw

COL = {'violet': (255, 68, 253), 'green': (20, 245, 60), 'blue': (20, 207, 253), 'orange': (255, 106, 60), 'white': (255, 255, 255)}


def render(rows_bytes, scale=6):
    """rows_bytes: list of rows, each a list of bytes. Returns an RGB array scale x larger (x pixel = 7 per byte)."""
    h, w = len(rows_bytes), 7 * len(rows_bytes[0])
    lit = np.zeros((h, w), bool)
    pal = np.zeros((h, w), bool)
    for y, row in enumerate(rows_bytes):
        for c, b in enumerate(row):
            for k in range(7):
                lit[y, c * 7 + k] = bool(b >> k & 1)
                pal[y, c * 7 + k] = bool(b & 0x80)
    img = np.zeros((h, w, 3), np.uint8)
    for y in range(h):
        for x in range(w):
            if not lit[y, x]:
                continue
            nb = (x > 0 and lit[y, x - 1]) or (x + 1 < w and lit[y, x + 1])
            if nb:
                c = COL['white']
            elif not pal[y, x]:
                c = COL['violet'] if x % 2 == 0 else COL['green']
            else:
                c = COL['blue'] if x % 2 == 0 else COL['orange']
            img[y, x] = c
    return np.kron(img, np.ones((scale, scale, 1), np.uint8))


def image_at(body, addr, width, rows):
    o = addr - 0x4000
    return [list(body[o + r * width: o + (r + 1) * width]) for r in range(rows)]


def word(body, a):
    return body[a - 0x4000] | body[a + 1 - 0x4000] << 8


def main(out='scratch/sheet.png'):
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    items = []
    for i in range(16):
        p = word(body, 0x96E8 + 2 * i)
        items.append(('96E8[%d]' % i, p, 3, 10))
    for i in range(32):
        p = word(body, 0x9E5C + 2 * i)
        items.append(('9E5C[%d]' % i, p, 3, 10))
    extra = [0x9A42, 0xA472, 0x9D8A, 0x96FA, 0x9970, 0x9CB8]
    items += [('$%04X' % a, a, 3, 10) for a in extra]
    items += [('big $82E0', 0x82E0, 4, 20), ('big $8330', 0x8330, 4, 20), ('big $8380', 0x8380, 4, 20), ('cart $854E', 0x854E, 3, 10), ('cart $856C', 0x856C, 3, 10)]
    items += [('fruit %d' % i, 0x6B6E + 36 * i, 3, 12) for i in range(9)]
    cell_w, cell_h, scale, per = 150, 150, 6, 8
    rows_n = (len(items) + per - 1) // per
    sheet = Image.new('RGB', (cell_w * per, cell_h * rows_n), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    for n, (name, addr, w, h) in enumerate(items):
        x0, y0 = (n % per) * cell_w, (n // per) * cell_h
        d.text((x0 + 4, y0 + 2), '%s -> $%04X' % (name, addr), fill=(200, 200, 200))
        if 0x4000 <= addr <= 0xA9FF - w * h:
            im = render(image_at(body, addr, w, h), scale)
            sheet.paste(Image.fromarray(im), (x0 + 4, y0 + 18))
        else:
            d.text((x0 + 4, y0 + 40), 'not a pointer', fill=(255, 80, 80))
    sheet.save(out)
    print('wrote', out, sheet.size, len(items), 'images')


if __name__ == '__main__':
    main(*sys.argv[1:2])
