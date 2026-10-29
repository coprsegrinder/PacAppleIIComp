import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from dis6502 import disassemble, OPS

def txt(b, org=0x1000):
    return [t for _, _, t in disassemble(bytes(b), org)]

# First 39 bytes of the PAC-MAN file body (hand-decoded in REVERSE_ENGINEERING.md).
BOOT = bytes.fromhex(
    "a9af8517a97f8515a000841684 14a270b1149116c8d0f9c617c615ca10f22089fe2093fe4c2740".replace(" ", ""))

class Dis(unittest.TestCase):
    def test_boot_prefix(self):
        t = txt(BOOT)
        self.assertEqual(t[:4], ["LDA #$AF", "STA $17", "LDA #$7F", "STA $15"])
        self.assertIn("LDA ($14),Y", t); self.assertIn("STA ($16),Y", t)
        self.assertEqual(t[-3:], ["JSR $FE89", "JSR $FE93", "JMP $4027"])
        self.assertEqual(t.count("BNE $1010"), 1)   # inner copy loop
        self.assertEqual(t.count("BPL $1010"), 1)   # outer page loop

    def test_branch_targets(self):
        self.assertEqual(txt([0xD0, 0xFE], 0x2000), ["BNE $2000"])
        self.assertEqual(txt([0xF0, 0x02], 0x2000), ["BEQ $2004"])

    def test_documented_opcode_count(self):
        self.assertEqual(len(OPS), 151)

    def test_unknown_and_truncated_are_bytes(self):
        self.assertEqual(txt([0x02]), [".byte $02"])
        self.assertEqual(txt([0xAD, 0x00]), [".byte $AD", "BRK"])  # $00 is BRK; $AD lacks its operand

if __name__ == '__main__':
    unittest.main()
