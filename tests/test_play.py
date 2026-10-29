import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine
from play import do_step


def mach(prog=b''):
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x300, bytes(prog))
    mc.cpu.pc = 0x300
    return mc


class Steps(unittest.TestCase):
    def test_keys(self):
        mc = mach()
        for st in ('key:sp', 'key:cr', 'key:1'):
            do_step(mc, st)
        self.assertEqual(mc.keys, [0xA0, 0x8D, 0xB1])

    def test_run_steps_instructions_and_reports_key_wait(self):
        mc = mach([0x20, 0x0C, 0xFD, 0xEA])                  # JSR $FD0C (RDKEY)
        msgs = []
        do_step(mc, '10', msgs.append)
        self.assertEqual(mc.cpu.pc, 0xFD0C)
        self.assertTrue(msgs and msgs[0].startswith('(waiting in RDKEY'))
        do_step(mc, 'key:x')
        do_step(mc, '5')
        self.assertEqual(mc.cpu.a, 0xF8)

    def test_underscore_counts_and_info_and_text(self):
        mc = mach([0xA9, 0xC8, 0x20, 0xF0, 0xFD] + [0xEA] * 5)      # LDA #'H' ; JSR COUT1
        out = []
        do_step(mc, '1_0', out.append)
        do_step(mc, 'text', out.append)
        do_step(mc, 'info', out.append)
        self.assertIn(' 0 H', out)
        self.assertTrue(out[-1].startswith('cycles '))

    def test_cpu_error_is_reported_not_raised(self):
        mc = mach([0x02])                                           # undocumented opcode
        out = []
        do_step(mc, '5', out.append)
        self.assertTrue(out and out[0].startswith('CPU error'))


if __name__ == '__main__':
    unittest.main()
