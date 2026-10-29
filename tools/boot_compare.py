"""Read-only comparison of the DOS boot tracks (0-2) of a 140K DOS 3.3-order image against a reference DOS 3.3 image, plus a scan for hidden data.
Usage: python -I tools/boot_compare.py <image.dsk> <reference.dsk>
Never writes to either file. Prints: per-sector identical/different for tracks 0-2, offsets of differences, VTOC facts, deleted catalog entries,
and non-zero sectors that no file or system area owns (possible hidden data).
"""
import sys


def sec(img, t, s):
    o = (t * 16 + s) * 256
    return img[o:o + 256]


def groups(offsets):
    out = []
    for x in sorted(offsets):
        if out and x == out[-1][1] + 1:
            out[-1][1] = x
        else:
            out.append([x, x])
    return out


def compare_boot(img, ref, tracks=(0, 1, 2)):
    """Returns {(t, s): sorted list of differing byte offsets} for every sector that differs."""
    diffs = {}
    for t in tracks:
        for s in range(16):
            a, b = sec(img, t, s), sec(ref, t, s)
            if a != b:
                diffs[(t, s)] = [i for i in range(256) if a[i] != b[i]]
    return diffs


def used_sectors(img):
    """Sectors owned by DOS (tracks 0-2), the VTOC/catalog chain and every file (its T/S list and data)."""
    used = {(t, s) for t in (0, 1, 2) for s in range(16)}
    used.add((17, 0))
    t, s, seen = img[(17 * 16) * 256 + 1], img[(17 * 16) * 256 + 2], set()
    while t and (t, s) not in seen:
        seen.add((t, s))
        used.add((t, s))
        c = sec(img, t, s)
        for i in range(7):
            e = c[11 + i * 35:11 + (i + 1) * 35]
            if e[0] in (0, 0xFF):
                continue
            tl, ts_, tsseen = e[0], e[1], set()
            while tl and (tl, ts_) not in tsseen and tl < 35:
                tsseen.add((tl, ts_))
                used.add((tl, ts_))
                lst = sec(img, tl, ts_)
                for k in range(12, 256, 2):
                    if lst[k] or lst[k + 1]:
                        used.add((lst[k], lst[k + 1]))
                tl, ts_ = lst[1], lst[2]
        t, s = c[1], c[2]
    return used


def deleted_entries(img):
    out = []
    t, s, seen = img[(17 * 16) * 256 + 1], img[(17 * 16) * 256 + 2], set()
    while t and (t, s) not in seen:
        seen.add((t, s))
        c = sec(img, t, s)
        for i in range(7):
            e = c[11 + i * 35:11 + (i + 1) * 35]
            if e[0] == 0xFF:
                out.append(''.join(chr(b & 0x7F) for b in e[3:33]).rstrip())
        t, s = c[1], c[2]
    return out


def hidden_data(img):
    used = used_sectors(img)
    return [(t, s, sum(1 for b in sec(img, t, s) if b)) for t in range(35) for s in range(16)
            if (t, s) not in used and any(sec(img, t, s))]


def main():
    img = open(sys.argv[1], 'rb').read()
    ref = open(sys.argv[2], 'rb').read()
    assert len(img) == len(ref) == 143360
    v = sec(img, 17, 0)
    r = sec(ref, 17, 0)
    print('VTOC (image / reference): DOS release %d / %d, volume %d / %d, tracks %d / %d, sectors per track %d / %d' % (
        v[3], r[3], v[6], r[6], v[0x34], r[0x34], v[0x35], r[0x35]))
    print('boot sector T0/S0: first 8 bytes image %s reference %s' % (sec(img, 0, 0)[:8].hex(' '), sec(ref, 0, 0)[:8].hex(' ')))
    d = compare_boot(img, ref)
    print('tracks 0-2: %d of 48 sectors identical to the reference, %d differ' % (48 - len(d), len(d)))
    for (t, s), offs in sorted(d.items()):
        a, b = sec(img, t, s), sec(ref, t, s)
        gs = groups(offs)
        print('  T%d/S%-2d %3d bytes differ in %d run(s): %s' % (t, s, len(offs), len(gs), ' '.join(
            '%02X-%02X(img %s ref %s)' % (x, y, a[x:min(y, x + 3) + 1].hex(), b[x:min(y, x + 3) + 1].hex()) for x, y in gs[:6])))
    print('deleted catalog entries:', deleted_entries(img))
    h = hidden_data(img)
    print('non-zero sectors owned by no file or system area: %d' % len(h), [('T%d/S%d' % (t, s), n) for t, s, n in h[:20]])
    used = used_sectors(img)
    print('sectors owned: %d of 560' % len(used))


if __name__ == '__main__':
    main()
