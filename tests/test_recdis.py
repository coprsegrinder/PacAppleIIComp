import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from recdis import analyse

class Rec(unittest.TestCase):
    def test_follows_branch_call_and_stops_at_rts(self):
        # 2000: JSR $2008 / 2003: BNE $2000 / 2005: RTS / 2006: data $FF $FF / 2008: LDA #1 / 200A: RTS
        body = bytes([0x20, 0x08, 0x20, 0xD0, 0xFB, 0x60, 0xFF, 0xFF, 0xA9, 0x01, 0x60])
        code, labels, ind, out, ov = analyse(body, 0x2000, [0x2000])
        self.assertEqual(sorted(code), [0x2000, 0x2003, 0x2005, 0x2008, 0x200A])
        self.assertNotIn(0x2006, code)                  # data after RTS is not reached
        self.assertEqual(labels, {0x2000, 0x2008})
        self.assertEqual((ind, out, ov), ([], set(), []))

    def test_outside_and_indirect(self):
        body = bytes([0x20, 0x89, 0xFE, 0x6C, 0x00, 0x03])  # JSR $FE89 (ROM) ; JMP ($0300)
        code, labels, ind, out, ov = analyse(body, 0x2000, [0x2000])
        self.assertEqual(out, {0xFE89}); self.assertEqual(ind, [0x2003])

    def test_invalid_opcode_stops_path(self):
        code, *_ = analyse(bytes([0xEA, 0x02, 0xEA]), 0x2000, [0x2000])
        self.assertEqual(sorted(code), [0x2000])

if __name__ == '__main__':
    unittest.main()
