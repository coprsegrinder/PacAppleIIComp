import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from classify_gaps import gaps_of, pointers_from_code
from recdis import analyse


class Gaps(unittest.TestCase):
    def test_gaps_of(self):
        self.assertEqual(gaps_of({0x4001, 0x4002, 0x4005}, 0x4000, 0x4008), [(0x4000, 0x4000), (0x4003, 0x4004), (0x4006, 0x4007)])

    def test_pointers_absolute_and_immediate_pair(self):
        # $4000: LDA $4020,X ; LDA #$30 ; LDY #$40 ; RTS      ($4020 absolute ref, $4030 via #lo/#hi pair)
        body = bytearray(0x40)
        body[0:10] = bytes([0xBD, 0x20, 0x40, 0xA9, 0x30, 0xA0, 0x40, 0x60, 0xEA, 0xEA])
        code, *_ = analyse(bytes(body), 0x4000, [0x4000])
        refs = pointers_from_code(bytes(body), code, 0x4000, 0x4040)
        self.assertIn(0x4020, refs)
        self.assertIn(0x4030, refs)
        self.assertNotIn(0x4000, refs)


if __name__ == '__main__':
    unittest.main()
