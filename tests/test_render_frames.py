import os, struct, sys, tempfile, unittest, zlib
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from render_frames import pixels, write_png, sheet, track, VIOLET, GREEN, BLUE, ORANGE, WHITE
from apple2 import Machine

BLACK = (0, 0, 0)


class Pixels(unittest.TestCase):
    def test_bit0_is_leftmost_pixel_and_isolated_colour_by_parity(self):
        img = pixels(bytes([0b00000001]), 1, 1, col=0)
        self.assertEqual(img[0][0], VIOLET)                     # even column, palette 0
        self.assertEqual(img[0][1:], [BLACK] * 6)
        self.assertEqual(pixels(bytes([0b00000010]), 1, 1, 0)[0][1], GREEN)   # odd column

    def test_palette_bit_selects_blue_orange(self):
        self.assertEqual(pixels(bytes([0x81]), 1, 1, 0)[0][0], BLUE)
        self.assertEqual(pixels(bytes([0x82]), 1, 1, 0)[0][1], ORANGE)

    def test_adjacent_pixels_are_white_and_column_parity_shifts_colour(self):
        self.assertEqual(pixels(bytes([0b00000011]), 1, 1, 0)[0][:2], [WHITE, WHITE])
        self.assertEqual(pixels(bytes([0b00000001]), 1, 1, col=1)[0][0], GREEN)   # 7 pixels per byte: odd column starts on odd pixel

    def test_row_major_layout(self):
        img = pixels(bytes([1, 0, 0, 1]), 2, 2, 0)              # 2 bytes wide, 2 rows
        self.assertNotEqual(img[0][0], BLACK)
        self.assertNotEqual(img[1][7], BLACK)


class Png(unittest.TestCase):
    def test_png_roundtrip_header_and_size(self):
        p = os.path.join(tempfile.mkdtemp(), 't.png')
        write_png(p, [[(1, 2, 3), (4, 5, 6)], [(7, 8, 9), (10, 11, 12)]])
        with open(p, 'rb') as f:
            d = f.read()
        self.assertEqual(d[:8], b'\x89PNG\r\n\x1a\n')
        w, h = struct.unpack('>II', d[16:24])
        self.assertEqual((w, h), (2, 2))
        i = d.index(b'IDAT')
        n = struct.unpack('>I', d[i - 4:i])[0]
        raw = zlib.decompress(d[i + 4:i + 4 + n])
        self.assertEqual(raw, b'\x00\x01\x02\x03\x04\x05\x06' + b'\x00\x07\x08\t\n\x0b\x0c')

    def test_sheet_places_frames(self):
        body = bytes(range(64))
        canvas, pos = sheet([{'base': 0x4000, 'x0': 0, 'w': 1, 'rows': 2, 'col': 0}], body, scale=2, width=40)
        self.assertEqual(pos, [(6, 6)])
        self.assertEqual(len(canvas[0]), 40)


class Track(unittest.TestCase):
    def test_rows_merge_into_one_blit(self):
        mc = Machine()
        mc.m[0x55B2], mc.m[0x55B3] = 0x00, 0x60               # base $6000
        mc.m[0x1A] = 3
        mc.cpu.pc = 0x55AD
        blits = track(mc)
        for row in range(3):                                   # three consecutive rows of a 3-wide blit
            mc.m[3], mc.m[0x10], mc.m[4] = row * 3, 20 + row, 7
            mc.cpu.pc = 0x55AD
            mc.m[0x55AD] = 0xEA                                # NOP so step() is harmless
            mc.cpu.step()
        self.assertEqual(len(blits), 1)
        self.assertEqual((blits[0]['rows'], blits[0]['w'], blits[0]['base'], blits[0]['x0']), (3, 3, 0x6000, 0))


if __name__ == '__main__':
    unittest.main()
