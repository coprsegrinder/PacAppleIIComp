"""TASK-028b: runs port/out pacman_view.exe (GDI window), which captures only its OWN window (PrintWindow) to a BMP,
and compares every pixel with the expected image of the built-in test page at scales 1 and 3 (exact integer replication, no smoothing).
Needs the build first: port\\build.bat. Skips with a message if the exe is missing. Run: python -I tests/test_window_shot.py"""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "out", "bat", "pacman_view.exe")
BLACK, VIOLET, GREEN, BLUE, ORANGE, WHITE = 0x000000, 0xFF44FD, 0x14F53C, 0x14CFFD, 0xFF6A3C, 0xFFFFFF


def expected_page():
    """Same layout as port/src/app/view_main.cpp (docs/APP_WINDOW.md), colours by the standard hi-res rules for isolated/adjacent pixels."""
    img = [[BLACK] * 280 for _ in range(192)]
    for y in range(32):
        for x in range(0, 280, 4):
            xx = x + (1 if y & 1 else 0)
            img[y][xx] = GREEN if y & 1 else VIOLET
    for y in range(32, 64):
        for x in range(0, 280, 4):
            xx = x + (1 if y & 1 else 0)
            img[y][xx] = ORANGE if y & 1 else BLUE
    for y in range(64, 96):
        for x in range(100, 140):
            img[y][x] = WHITE
    for x in range(280):
        img[100][x] = WHITE
        img[191][x] = WHITE
    for y in range(101, 191):  # one-pixel vertical lines have no horizontal neighbour: isolated, so coloured by parity (not white)
        img[y][0] = VIOLET
        img[y][279] = GREEN
    return img


def read_bmp(path):
    d = open(path, "rb").read()
    off, = struct.unpack_from("<I", d, 10)
    w, h, planes, bpp = struct.unpack_from("<iiHH", d, 18)
    assert bpp == 32 and h < 0, (bpp, h)
    h = -h
    px = struct.unpack_from("<%dI" % (w * h), d, off)
    return w, h, [[px[y * w + x] & 0xFFFFFF for x in range(w)] for y in range(h)]


class WindowShot(unittest.TestCase):
    def shot(self, scale):
        if not os.path.exists(EXE):
            self.skipTest("build port\\build.bat first")
        with tempfile.TemporaryDirectory() as t:
            out = os.path.join(t, "s.bmp")
            r = subprocess.run([EXE, "--scale", str(scale), "--shot", out], timeout=30)
            self.assertEqual(r.returncode, 0)
            return read_bmp(out)

    def check(self, scale):
        w, h, got = self.shot(scale)
        self.assertEqual((w, h), (280 * scale, 192 * scale))
        exp = expected_page()
        bad = sum(1 for y in range(h) for x in range(w) if got[y][x] != exp[y // scale][x // scale])
        self.assertEqual(bad, 0, "%d of %d pixels differ" % (bad, w * h))

    def test_scale_1(self):
        self.check(1)

    def test_scale_3(self):
        self.check(3)

    def test_bad_argument_exits_nonzero(self):
        if not os.path.exists(EXE):
            self.skipTest("build first")
        self.assertEqual(subprocess.run([EXE, "--nonsense"], timeout=15).returncode, 64)


class RealMazeShot(unittest.TestCase):
    """TASK-028c: the window shows the REAL maze (walls from a data pack built from the user's own disk image) in colour. The expected image comes from
    the independent Python wall model (tools/wall_model.py, verified against the oracle) plus the standard colour rules. Needs the disk image,
    scratch/extract (original data, git-ignored) and the build; skips otherwise."""

    def test_maze_scale_3(self):
        dsk = os.path.join(ROOT, "Pacman (1983)(Atari).dsk")
        body = os.path.join(ROOT, "scratch", "extract", "PAC-MAN.1000.bin")
        pk = os.path.join(ROOT, "out", "bat", "pacman_pack.exe")
        if not all(os.path.exists(f) for f in (EXE, dsk, body, pk)):
            self.skipTest("needs the build, the disk image and scratch/extract")
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        import wall_model
        data = open(body, "rb").read()
        table = data[0x5F69 - 0x4000: 0x5F69 - 0x4000 + 426]
        page = bytearray(wall_model.wall_page(table, 1))
        # independent dot model: fresh-game map at $65BA, small dot table $61A8, energizer tile table $61CA (docs/GRAPHICS.md)
        mp = data[0x65BA - 0x4000: 0x65BA - 0x4000 + 896]
        dt = data[0x61A8 - 0x4000: 0x61A8 - 0x4000 + 14]
        tt = data[0x61CA - 0x4000: 0x61CA - 0x4000 + 112]
        ra = lambda y: (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28
        for c in range(27):
            for r in range(1, 31):
                cell, x, y = mp[c * 32 + r], 59 + 6 * c, 11 + 6 * (r - 1)
                if cell == 0:
                    continue
                if cell & 1:
                    for k in range(2):
                        for b in range(2):
                            page[ra(y + k) + x // 7 + b] |= dt[(x % 7) * 2 + b]
                else:
                    for k in range(8):
                        for b in range(2):
                            page[ra(y - 1 + k) + (x - 1) // 7 + b] |= tt[((x - 1) % 7) * 16 + k * 2 + b]
        img = page_to_colours(page)
        with tempfile.TemporaryDirectory() as t:
            pack, out = os.path.join(t, "p.pmpk"), os.path.join(t, "m.bmp")
            self.assertEqual(subprocess.run([pk, "build", dsk, pack], timeout=60, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run([EXE, "--pack", pack, "--scale", "3", "--shot", out], timeout=30).returncode, 0)
            w, h, got = read_bmp(out)
            # a damaged pack must be refused (exit 3) and never show a window
            bad = bytearray(open(pack, "rb").read())
            bad[len(bad) // 2] ^= 1
            open(pack, "wb").write(bad)
            self.assertEqual(subprocess.run([EXE, "--pack", pack, "--shot", os.path.join(t, "x.bmp")], timeout=30, capture_output=True).returncode, 3)
        self.assertEqual((w, h), (840, 576))
        diff = sum(1 for y in range(h) for x in range(w) if got[y][x] != img[y // 3][x // 3])
        self.assertEqual(diff, 0, "%d pixels differ" % diff)
        self.assertGreater(sum(1 for r in img for v in r if v != BLACK), 1000)  # the maze is really there


def page_to_colours(page):
    """Standard hi-res rules from the 8192-byte page (independent of the C++ code)."""
    lit = [[0] * 280 for _ in range(192)]
    pal = [[0] * 280 for _ in range(192)]
    for y in range(192):
        a = 0x2000 + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28 - 0x2000
        for c in range(40):
            b = page[a + c]
            for k in range(7):
                lit[y][c * 7 + k] = b >> k & 1
                pal[y][c * 7 + k] = b >> 7
    out = [[BLACK] * 280 for _ in range(192)]
    for y in range(192):
        for x in range(280):
            if not lit[y][x]:
                continue
            l = x > 0 and lit[y][x - 1]
            r = x < 279 and lit[y][x + 1]
            if l or r:
                out[y][x] = WHITE
            elif pal[y][x]:
                out[y][x] = ORANGE if x & 1 else BLUE
            else:
                out[y][x] = GREEN if x & 1 else VIOLET
    return out


if __name__ == "__main__":
    unittest.main()
