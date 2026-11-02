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


if __name__ == "__main__":
    unittest.main()
