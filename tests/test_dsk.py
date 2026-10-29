import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import dsk

def fake_image():
    """Tiny synthetic disk: catalog T17/S15 -> one binary file 'TEST' with 2 data sectors."""
    img = bytearray(143360)
    sec = lambda t, s: (t * 16 + s) * 256
    img[sec(17, 0) + 1:sec(17, 0) + 3] = bytes([17, 15])           # VTOC -> first catalog sector
    ent = sec(17, 15) + 11
    img[ent:ent + 3] = bytes([18, 0, 0x04])                         # T/S list at T18/S0, type binary
    img[ent + 3:ent + 33] = bytes(ord(c) | 0x80 for c in 'TEST'.ljust(30))
    tsl = sec(18, 0) + 12
    img[tsl:tsl + 4] = bytes([19, 0, 19, 1])                        # data sectors T19/S0, T19/S1
    img[sec(19, 0):sec(19, 0) + 4] = bytes([0x00, 0x20, 0x03, 0x00])  # load $2000, length 3
    img[sec(19, 0) + 4:sec(19, 0) + 7] = b'ABC'
    return bytes(img)

class Dsk(unittest.TestCase):
    def test_extract_synthetic(self):
        addr, n, body, tail = dsk.binary_body(fake_image(), 'TEST')
        self.assertEqual((addr, n, body), (0x2000, 3, b'ABC'))
        self.assertEqual(len(tail), 2 * 256 - 4 - 3)

    def test_missing_file(self):
        with self.assertRaises(KeyError): dsk.read_file(fake_image(), 'NOPE')

if __name__ == '__main__':
    unittest.main()
