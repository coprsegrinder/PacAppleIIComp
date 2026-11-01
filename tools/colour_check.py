"""TASK-027e: check the hi-res colour model against an AppleWin window screenshot taken in the same paused state as a memory dump
(scratch/aw_play.png + scratch/gp.bin, 2026-10-29; development tool, original data from scratch/, never committed).
Colour model (standard Apple II): a lit pixel with a lit horizontal neighbour is white; an isolated lit pixel is violet (even x) / green (odd x)
when the byte's palette bit 7 = 0, blue (even x) / orange (odd x) when it is 1; unlit pixels are black.
Method: build the predicted class of every one of the 280x192 pixels from the dumped page, find the screenshot offset/scale that makes the lit
mask fit best, then classify the observed colour (hue) of each isolated lit pixel and report the agreement and the confusion matrix.
Usage: python -I tools/colour_check.py [png] [dump]"""
import colorsys, sys
import numpy as np
from PIL import Image

NAMES = ['violet', 'green', 'blue', 'orange', 'white']
REF_HUE = {'violet': 290, 'green': 125, 'blue': 190, 'orange': 18}


def row_address(y):
    return 0x2000 + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


def pixels(page):
    """lit[192][280], pal[192][280] from the 8,192-byte page (page[0] = $2000)."""
    lit = np.zeros((192, 280), bool)
    pal = np.zeros((192, 280), bool)
    for y in range(192):
        a = row_address(y) - 0x2000
        for c in range(40):
            b = page[a + c]
            for k in range(7):
                lit[y, c * 7 + k] = bool(b >> k & 1)
                pal[y, c * 7 + k] = bool(b & 0x80)
    return lit, pal


def predicted(lit, pal, flip=False):
    cls = np.full(lit.shape, -1)
    left = np.zeros_like(lit); left[:, 1:] = lit[:, :-1]
    right = np.zeros_like(lit); right[:, :-1] = lit[:, 1:]
    iso = lit & ~left & ~right
    xs = np.arange(280)[None, :].repeat(192, 0)
    odd = (xs & 1).astype(bool) ^ flip
    cls[iso & ~pal & ~odd] = 0
    cls[iso & ~pal & odd] = 1
    cls[iso & pal & ~odd] = 2
    cls[iso & pal & odd] = 3
    cls[lit & (left | right)] = 4
    return cls


def best_fit(img, lit):
    """Find (ox, oy, sx, sy) so that the mean brightness of lit pixels' cells is highest relative to unlit cells."""
    g = img.mean(axis=2)
    best = None
    ys, xs = np.nonzero(lit)
    sel = np.random.default_rng(1).choice(len(ys), min(4000, len(ys)), replace=False)
    for s in np.arange(4.00, 4.10, 0.01):
        for oy in range(0, 12):
            for ox in range(0, 12):
                px = (ox + (xs[sel] + .5) * s).astype(int)
                py = (oy + (ys[sel] + .5) * s).astype(int)
                ok = (px < g.shape[1]) & (py < g.shape[0])
                v = g[py[ok], px[ok]].mean()
                if best is None or v > best[0]:
                    best = (v, ox, oy, s)
    return best


def hue_class(rgb):
    r, g, b = [c / 255 for c in rgb]
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    if v < 0.35:
        return None
    if s < 0.30:
        return 'white'
    deg = h * 360
    return min(REF_HUE, key=lambda n: min(abs(deg - REF_HUE[n]), 360 - abs(deg - REF_HUE[n])))


def main(png='scratch/aw_play.png', dump='scratch/gp.bin'):
    page = open(dump, 'rb').read()[0x2000:0x4000]
    img = np.asarray(Image.open(png).convert('RGB')).astype(float)
    lit, pal = pixels(page)
    v, ox, oy, s = best_fit(img, lit)
    print('fit: offset (%d,%d) scale %.2f, mean brightness of lit pixels %.0f' % (ox, oy, s, v))
    result = {}
    for flip in (False, True):
        cls = predicted(lit, pal, flip)
        conf = {}
        total = agree = 0
        for y in range(192):
            for x in range(280):
                if cls[y, x] < 0 or cls[y, x] == 4:
                    continue
                cx, cy = int(ox + (x + .5) * s), int(oy + (y + .5) * s)
                patch = img[max(cy - 1, 0): cy + 2, max(cx - 1, 0): cx + 2].reshape(-1, 3)
                obs = hue_class(patch[patch.sum(1).argmax()])    # the brightest sample of the cell: robust against the blur
                if obs is None:
                    continue
                total += 1
                exp = NAMES[cls[y, x]]
                conf[(exp, obs)] = conf.get((exp, obs), 0) + 1
                agree += exp == obs
        print('parity %s: isolated lit pixels classified %d, agree %d (%.1f%%)' % ('FLIPPED' if flip else 'standard', total, agree, 100 * agree / max(total, 1)))
        for k in sorted(conf):
            print('   expected %-7s observed %-7s %6d' % (k[0], k[1], conf[k]))
        result[flip] = (total, agree)
    return result


if __name__ == '__main__':
    main(*sys.argv[1:3])
