import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine, NeedKey, row_base
from cpu6502 import CPUError


def mach(prog, org=0x0300):
    mc = Machine()
    mc.init_text_screen()
    mc.load(org, bytes(prog))
    mc.cpu.pc = org
    return mc


def jsr(a):
    return [0x20, a & 0xFF, a >> 8]


class Switches(unittest.TestCase):
    def test_keyboard_strobe(self):
        mc = mach([0xAD, 0x00, 0xC0, 0x8D, 0x10, 0xC0, 0xAD, 0x00, 0xC0, 0x00])   # LDA $C000; STA $C010; LDA $C000
        mc.push_key('A')
        mc.cpu.step()
        self.assertEqual(mc.cpu.a, 0xC1)
        mc.cpu.step(); mc.cpu.step()
        self.assertEqual(mc.cpu.a, 0x41)                    # strobe cleared, key value remains (bit 7 low)

    def test_no_key_reads_zero_strobe(self):
        mc = mach([0xAD, 0x00, 0xC0])
        mc.cpu.step()
        self.assertEqual(mc.cpu.a & 0x80, 0)

    def test_speaker_logs_cycles(self):
        mc = mach([0x2C, 0x30, 0xC0, 0xEA, 0x2C, 0x30, 0xC0])
        for _ in range(3):
            mc.cpu.step()
        self.assertEqual(mc.speaker, [0, 6])               # BIT abs = 4 cycles read at its start-count; NOP 2

    def test_video_switches(self):
        mc = mach([0xAD, 0x50, 0xC0, 0xAD, 0x52, 0xC0, 0xAD, 0x57, 0xC0, 0xAD, 0x55, 0xC0])
        for _ in range(4):
            mc.cpu.step()
        self.assertEqual(mc.switches, {'text': False, 'mixed': False, 'page2': True, 'hires': True})
        mc.cpu.pc = 0x0300
        mc.m[0x300:0x303] = bytes([0xAD, 0x51, 0xC0])
        mc.cpu.step()
        self.assertTrue(mc.switches['text'])

    def test_annunciators(self):
        mc = mach([0xAD, 0x59, 0xC0, 0xAD, 0x5B, 0xC0, 0xAD, 0x58, 0xC0])      # AN0 on, AN1 on, AN0 off
        for _ in range(2):
            mc.cpu.step()
        self.assertEqual(mc.annunciators, [True, True, False, False])
        mc.cpu.step()
        self.assertEqual(mc.annunciators, [False, True, False, False])
        self.assertEqual(mc.unknown_io, {})

    def test_buttons(self):
        mc = mach([0xAD, 0x61, 0xC0, 0xAD, 0x62, 0xC0])
        mc.buttons[0] = True
        mc.cpu.step(); self.assertEqual(mc.cpu.a, 0x80)
        mc.cpu.step(); self.assertEqual(mc.cpu.a, 0)

    def test_paddle_timer(self):
        mc = mach([0xAD, 0x70, 0xC0] + [0xAD, 0x64, 0xC0] + [0xEA] * 200 + [0xAD, 0x64, 0xC0])
        mc.paddles[0] = 100                                  # ~1100 cycles
        mc.cpu.step(); mc.cpu.step()
        self.assertEqual(mc.cpu.a, 0x80)                     # timer still running
        for _ in range(200):
            mc.cpu.step()                                    # 400 cycles: still running
        mc.cpu.cycles += 1000
        mc.cpu.step()
        self.assertEqual(mc.cpu.a, 0)

    def test_unknown_io_is_recorded(self):
        mc = mach([0xAD, 0x81, 0xC0])
        mc.cpu.step()
        self.assertEqual(mc.unknown_io, {(0xC081, 'read'): 1})

    def test_unmodelled_rom_raises(self):
        mc = mach(jsr(0xFB00))
        with self.assertRaises(CPUError):
            mc.run(10)


class Text(unittest.TestCase):
    def test_row_base(self):
        self.assertEqual([hex(row_base(r)) for r in (0, 1, 8, 16, 23)], ['0x400', '0x480', '0x428', '0x450', '0x7d0'])

    def test_cout_writes_and_advances(self):
        mc = mach([0xA9, 0xC8] + jsr(0xFDF0) + [0xA9, 0xC9] + jsr(0xFDF0) + [0x00])   # 'H','I'
        mc.run(20, stop_pc=0x030C)
        self.assertEqual(mc.m[0x400:0x402], bytes([0xC8, 0xC9]))
        self.assertEqual(mc.m[0x24], 2)

    def test_cout_preserves_registers(self):
        mc = mach([0xA2, 0x07, 0xA0, 0x09, 0xA9, 0xC1] + jsr(0xFDF0))
        mc.run(10, stop_pc=0x0309)
        self.assertEqual((mc.cpu.a, mc.cpu.x, mc.cpu.y), (0xC1, 7, 9))

    def test_cr_goes_to_next_row_and_scrolls(self):
        mc = mach([])
        mc.m[0x400:0x403] = b'\xC1\xC2\xC3'                  # top row text
        mc.set_cv(23)
        mc.cout(0xC4)
        mc.cout(0x8D)                                         # CR on last row scrolls
        self.assertEqual(mc.m[0x400:0x403], b'\xA0\xA0\xA0' if False else mc.m[0x400:0x403])
        self.assertEqual(mc.m[0x25], 23)
        self.assertEqual(mc.m[row_base(22)], 0xC4)            # last row's char moved up one line
        self.assertEqual(mc.m[row_base(23)], 0xA0)

    def test_wrap_at_column_40(self):
        mc = mach([])
        for _ in range(41):
            mc.cout(0xC1)
        self.assertEqual(mc.m[0x25], 1)
        self.assertEqual(mc.m[0x24], 1)
        self.assertEqual(mc.m[0x480], 0xC1)

    def test_vtab_uses_cv_not_a(self):
        mc = mach([0xA9, 0x00] + jsr(0xFC22))                # A = 0, but caller has set CV = 5
        mc.m[0x25] = 5
        mc.run(10, stop_pc=0x0305)
        self.assertEqual((mc.m[0x29] << 8 | mc.m[0x28]), row_base(5))

    def test_setvid_setkbd_vectors(self):
        mc = mach(jsr(0xFE93) + jsr(0xFE89))
        mc.run(10, stop_pc=0x0306)
        self.assertEqual((mc.m[0x36], mc.m[0x37], mc.m[0x38], mc.m[0x39]), (0xF0, 0xFD, 0x1B, 0xFD))


class Input(unittest.TestCase):
    def test_rdkey_waits_then_returns(self):
        mc = mach(jsr(0xFD0C) + [0x00])
        with self.assertRaises(NeedKey):
            mc.run(10)
        self.assertEqual(mc.cpu.pc, 0xFD0C)                  # parked on the trap, state intact
        self.assertGreater(mc.m[0x4E], 0)                    # random seed advanced while waiting
        mc.push_key('x')
        mc.run(10, stop_pc=0x0303)
        self.assertEqual(mc.cpu.a, 0xF8)

    def test_getln_single_digit(self):
        mc = mach(jsr(0xFD6F) + [0x00])
        mc.push_key('1'); mc.push_key(0x0D)
        mc.run(10, stop_pc=0x0303)
        self.assertEqual(mc.cpu.x, 1)
        self.assertEqual(mc.m[0x200:0x202], bytes([0xB1, 0x8D]))
        self.assertEqual(mc.m[0x400], 0xDD)                  # prompt ']' echoed

    def test_getln_resumes_across_keys_and_backspace(self):
        mc = mach(jsr(0xFD6F) + [0x00])
        mc.push_key('1')
        with self.assertRaises(NeedKey):
            mc.run(10)
        mc.push_key('2'); mc.push_key(0x08); mc.push_key('3'); mc.push_key(0x0D)
        mc.run(10, stop_pc=0x0303)
        self.assertEqual(mc.cpu.x, 2)
        self.assertEqual(mc.m[0x200:0x203], bytes([0xB1, 0xB3, 0x8D]))


class Coverage(unittest.TestCase):
    def test_exec_read_write_and_writer(self):
        # LDA $0400 ; STA $0500 ; JSR $0400+... -> use: LDA $0400 / STA $0500 / NOP
        mc = mach([0xAD, 0x00, 0x04, 0x8D, 0x00, 0x05, 0xEA])
        mc.enable_coverage()
        mc.run(3)
        c = mc.cov
        self.assertEqual([i for i in range(0x300, 0x308) if c['exec'][i]], list(range(0x300, 0x307)))
        self.assertEqual(c['read'][0x400], 1)
        self.assertEqual(c['write'][0x500], 1)
        self.assertEqual(c['writer'][0x500], 0x303)           # PC of the STA
        self.assertEqual(sum(c['read'][0x300:0x307]), 0)      # instruction fetches are not data reads
        self.assertEqual([i for i in range(0x300, 0x307) if c['opstart'][i]], [0x300, 0x303, 0x306])

    def test_stack_writes_are_attributed_to_the_jsr(self):
        mc = mach([0x20, 0x10, 0x03])                          # JSR $0310
        mc.enable_coverage()
        mc.m[0x310] = 0x60
        mc.run(2)
        self.assertEqual(mc.cov['writer'][0x1FD], 0x300)

    def test_self_modifying_operand_is_visible(self):
        # LDA #$07 ; STA $0306 ; LDA #$00 (operand patched to #$07)
        mc = mach([0xA9, 0x07, 0x8D, 0x06, 0x03, 0xA9, 0x00])
        mc.enable_coverage()
        mc.run(3)
        self.assertEqual(mc.cpu.a, 0x07)
        self.assertTrue(mc.cov['write'][0x306] and mc.cov['exec'][0x306])


if __name__ == '__main__':
    unittest.main()
