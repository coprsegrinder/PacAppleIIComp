import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine
from trace_tables import install_tracker


class Tracker(unittest.TestCase):
    def test_patched_base_and_offsets(self):
        mc = Machine()
        # $0300: LDA #$00 ; STA $030B (patch operand lo) ; LDA #$40 ; STA $030C ; LDX #0 ; LDA $0666,X ; INX ; LDA $0666,X  (reader at $030A)
        prog = [0xA9, 0x10, 0x8D, 0x0B, 0x03, 0xA9, 0x40, 0x8D, 0x0C, 0x03, 0xBD, 0x66, 0x06, 0xE8, 0xBD, 0x66, 0x06]
        mc.load(0x300, bytes(prog))
        mc.cpu.pc = 0x300
        # the reader instruction starts at $030A; the second LDA at $030E keeps base $0666
        mc.enable_coverage()
        stats = install_tracker(mc, {0x30A: 'reader'})
        mc.run(7)
        self.assertEqual(stats, {(0x30A, 0x4010): [1, 0, 0]})   # base patched to $4010, offset 0 read once
        # the unpatched second LDA is not in `readers`, so it is ignored


if __name__ == '__main__':
    unittest.main()
