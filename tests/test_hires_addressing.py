"""TASK-027c: the hi-res addressing tables of the original equal the standard Apple II formulas (so the C++ layer can compute instead of look up).
Needs scratch/extract (original data, never committed). Run: python -I tests/test_hires_addressing.py"""
import os, unittest

BODY = None


def tab(addr, n):
    return BODY[addr - 0x4000: addr - 0x4000 + n]


def row_address(y):
    return 0x2000 + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


class HiresAddressing(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        global BODY
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            raise unittest.SkipTest('scratch/extract not present')
        BODY = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()

    def test_row_tables_5B67_5C27_give_the_standard_row_address_for_rows_0_to_191(self):
        lo, hi = tab(0x5B67, 192), tab(0x5C27, 192)
        for y in range(192):
            self.assertEqual((hi[y] | 0x20) << 8 | lo[y], row_address(y), y)       # $5B58 ORs $20 into the high byte

    def test_pixel_tables_5A58_and_5958_are_x_div_7_and_x_mod_7(self):
        col, sh = tab(0x5A58, 256), tab(0x5958, 256)
        for x in range(256):
            self.assertEqual((col[x], sh[x]), (x // 7, x % 7), x)

    def test_x_of_256_or_more_adds_4_to_the_low_byte_and_36_to_the_column(self):
        # $5933: if the high byte $0D is nonzero, X = $0C + 4, column = table[X] + $24, shift = table[X]  (256 = 36 * 7 + 4)
        col, sh = tab(0x5A58, 256), tab(0x5958, 256)
        for lo in range(252):                       # x = 256 + lo, lo + 4 must stay inside the 256-entry tables
            x = 256 + lo
            self.assertEqual((col[lo + 4] + 0x24, sh[lo + 4]), (x // 7, x % 7), x)

    def test_pixel_masks_5925_and_592C_are_bit_and_inverse_bit_for_shifts_0_to_6(self):
        self.assertEqual(list(tab(0x5925, 7)), [1 << i for i in range(7)])
        self.assertEqual(list(tab(0x592C, 7)), [(~(1 << i)) & 0xFF for i in range(7)])


if __name__ == '__main__':
    unittest.main()
