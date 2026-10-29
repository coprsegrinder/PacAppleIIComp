import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine
from trace_callsites import install
from render_screen import hires_base, screen_rows


class Callsites(unittest.TestCase):
    def test_caller_is_the_jsr_address_and_params_are_logged(self):
        mc = Machine()
        mc.m[0x7000:0x7003] = bytes([0x20, 0x20, 0x6A])          # JSR $6A20 at $7000
        mc.m[0x6A20] = 0xEA                                      # NOP at the entry
        mc.m[0x1A], mc.m[0x10], mc.m[0x04] = 3, 40, 12
        mc.cpu.pc, mc.cpu.a, mc.cpu.y = 0x7000, 0x34, 0x92       # base $9234
        sites = install(mc)
        mc.cpu.step()                                            # JSR
        mc.cpu.step()                                            # NOP at entry (hook runs first)
        s = sites[(0x6A20, 0x7000)]
        self.assertEqual((s['n'], list(s['bases']), s['rows'], s['cols']), (1, [(0x9234, 3, 0)], {40}, {12}))

    def test_loop_back_to_55a6_is_not_a_new_blit(self):
        mc = Machine()
        mc.m[0x55B2], mc.m[0x55B3] = 0x00, 0x80
        mc.m[0x55A6] = 0xEA
        mc.m[0x55C4] = 0xEA                                      # pretend previous instruction was the loop-back at $55C4
        mc.m[0x7000:0x7003] = bytes([0x20, 0xA6, 0x55])
        mc.cpu.sp = 0xFD
        sites = install(mc)
        mc.cpu.pc = 0x7000
        mc.cpu.step()
        mc.cpu.step()                                            # entry via JSR: counted
        mc.cpu.pc = 0x55C4
        mc.cpu.step()                                            # NOP at $55C4 -> pc $55C5 (no real jump in this fake)
        mc.cpu.pc = 0x55A6
        mc.cpu.step()                                            # reached from $55C4's slot: not counted? prev pc was $55C4
        self.assertEqual(sites[(0x55A6, 0x7000)]['n'], 1)


class Screen(unittest.TestCase):
    def test_hires_base_addresses(self):
        self.assertEqual([hex(hires_base(y)) for y in (0, 1, 8, 64, 191)], ['0x2000', '0x2400', '0x2080', '0x2028', '0x3fd0'])

    def test_screen_rows_size(self):
        rows = screen_rows(bytearray(65536), scale=2)
        self.assertEqual((len(rows), len(rows[0])), (384, 560))


if __name__ == '__main__':
    unittest.main()
