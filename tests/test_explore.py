import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine
from explore import send, until_key, text_page2, CTRL_V, CTRL_C, ESC, CR


def mach(prog):
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x300, bytes(prog))
    mc.cpu.pc = 0x300
    return mc


class Helpers(unittest.TestCase):
    def test_constants_match_the_game_keycodes(self):
        # the game compares high-bit-set codes: Ctrl-V = $96, Ctrl-C = $83, Esc = $9B, Enter = $8D
        self.assertEqual([c | 0x80 for c in (CTRL_V, CTRL_C, ESC, CR)], [0x96, 0x83, 0x9B, 0x8D])

    def test_send_accepts_chars_and_ints(self):
        mc = mach([])
        send(mc, ['A', 0x0D, ' '])
        self.assertEqual(mc.keys, [0xC1, 0x8D, 0xA0])

    def test_until_key_reports_wait_and_limit(self):
        mc = mach([0x20, 0x0C, 0xFD])                       # JSR RDKEY: waits
        self.assertTrue(until_key(mc, 100))
        mc2 = mach([0x4C, 0x00, 0x03])                       # endless loop: never waits
        self.assertFalse(until_key(mc2, 100))

    def test_text_page2_reads_only_page_2(self):
        mc = mach([])
        mc.m[0x800:0x803] = bytes([0xC8, 0xC9, 0xA1])        # 'H','I','!' with high bit set
        mc.m[0x400:0x402] = bytes([0xD8, 0xD9])               # page 1 text must be ignored
        self.assertEqual(text_page2(mc), ['HI!'])


if __name__ == '__main__':
    unittest.main()
