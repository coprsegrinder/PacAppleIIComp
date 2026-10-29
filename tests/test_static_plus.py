import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from static_plus import table_targets, bytes_of
from recdis import analyse


class StaticPlus(unittest.TestCase):
    def test_table_targets_reads_little_endian_words(self):
        body = bytes([0xEA] * 4 + [0x1E, 0x8E, 0x28, 0x8E, 0x00, 0x90])
        self.assertEqual(table_targets(body, origin=0x4000, table=0x4004, count=3), [0x8E1E, 0x8E28, 0x9000])

    def test_extra_entry_finds_code_unreachable_from_main_entry(self):
        # $4000: JMP $4000 (loop). $4010: LDA #1 ; RTS  -- only reachable through an extra entry point
        body = bytearray(0x20)
        body[0:3] = bytes([0x4C, 0x00, 0x40])
        body[0x10:0x13] = bytes([0xA9, 0x01, 0x60])
        base, *_ = analyse(bytes(body), 0x4000, [0x4000])
        plus, *_ = analyse(bytes(body), 0x4000, [0x4000, 0x4010])
        self.assertEqual(len(bytes_of(base)), 3)
        self.assertEqual(sorted(bytes_of(plus) - bytes_of(base)), [0x4010, 0x4011, 0x4012])


if __name__ == '__main__':
    unittest.main()
