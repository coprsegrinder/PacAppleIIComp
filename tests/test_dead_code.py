"""Static proofs for TASK-021b: some unexecuted bytes can never run. Needs scratch/extract. Run: python -I tests/test_dead_code.py"""
import os, sys, unittest

BODY = None


def byte(a):
    return BODY[a - 0x4000]


class DeadCode(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        global BODY
        with open('scratch/extract/PAC-MAN.1000.bin', 'rb') as f:
            BODY = f.read()

    def test_two_carry_fixups_follow_immediate_additions_that_cannot_carry(self):
        # pattern: CLC; LDA #$3A; ADC #k; STA $16; BCC +2; INC $17   (INC at the listed address)
        for inc in (0x5754, 0x57CF):
            self.assertEqual((byte(inc), byte(inc + 1)), (0xE6, 0x17), hex(inc))     # INC $17
            self.assertEqual((byte(inc - 2), byte(inc - 1)), (0x90, 0x02), hex(inc))  # BCC +2
            self.assertEqual((byte(inc - 4), byte(inc - 3)), (0x85, 0x16), hex(inc))  # STA $16
            self.assertEqual(byte(inc - 6), 0x69, hex(inc))                           # ADC #k
            k = byte(inc - 5)
            self.assertEqual((byte(inc - 8), byte(inc - 7)), (0xA9, 0x3A), hex(inc))  # LDA #$3A
            self.assertLess(0x3A + k, 0x100, hex(inc))                                # the sum never carries

    def test_four_other_carry_fixups_add_to_a_computed_address_so_they_are_not_proven_dead(self):
        # pattern: LDA $16; ADC #$14; STA $16; BCC +2; INC $17: carries when $16 >= $EC (depends on the address from $7819)
        for inc in (0x5770, 0x57AA, 0x57EB, 0x5825):
            self.assertEqual((byte(inc - 8), byte(inc - 7)), (0xA5, 0x16), hex(inc))
            self.assertEqual((byte(inc - 6), byte(inc - 5)), (0x69, 0x14), hex(inc))

    def test_always_taken_branches_hide_data(self):
        # LDA #0 then BEQ (6DEE and 8A3A), LDA #$62 / CLC / ADC #3 / STA then BNE (53B2): the byte after the branch is never reached
        self.assertEqual((byte(0x6DE8), byte(0x6DE9), byte(0x6DEC)), (0xA9, 0x00, 0xF0))
        self.assertEqual(byte(0x6DEE), 0x60)
        self.assertEqual((byte(0x8A34), byte(0x8A35), byte(0x8A38)), (0xA9, 0x00, 0xF0))
        self.assertEqual((byte(0x53A9), byte(0x53AA), byte(0x53AB), byte(0x53AC), byte(0x53AD), byte(0x53B0)), (0xA9, 0x62, 0x18, 0x69, 0x03, 0xD0))
        self.assertNotEqual(0x62 + 3, 0)                                                # A is nonzero, so the BNE always branches


if __name__ == '__main__':
    unittest.main()
