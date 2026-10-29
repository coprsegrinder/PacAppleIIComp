import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from cpu6502 import CPU, RAM, CPUError, N, V, B, D, I, Z, C


def make(prog, org=0x0200, **regs):
    ram = RAM()
    ram.m[org:org + len(prog)] = bytes(prog)
    cpu = CPU(ram)
    cpu.pc = org
    for k, v in regs.items():
        setattr(cpu, k, v)
    return cpu


def run(prog, steps=1, **regs):
    cpu = make(prog, **regs)
    for _ in range(steps):
        cpu.step()
    return cpu


def flags(cpu):
    return {k: bool(cpu.p & m) for k, m in dict(N=N, V=V, D=D, Z=Z, C=C).items()}


class Basics(unittest.TestCase):
    def test_load_flags(self):
        self.assertTrue(run([0xA9, 0x00]).p & Z)
        self.assertTrue(run([0xA9, 0x80]).p & N)
        self.assertEqual(run([0xA2, 0x7F]).x, 0x7F)

    def test_cmp_flags(self):
        c = run([0xC9, 0x40], a=0x40)
        self.assertEqual((bool(c.p & Z), bool(c.p & C)), (True, True))
        c = run([0xC9, 0x40], a=0x30)
        self.assertEqual((bool(c.p & C), bool(c.p & N)), (False, True))   # 0x30-0x40 = 0xF0

    def test_shifts_and_rotates(self):
        c = run([0x0A], a=0x81)                       # ASL A
        self.assertEqual((c.a, bool(c.p & C)), (0x02, True))
        c = run([0x6A], a=0x01, p=0x20 | C)           # ROR A with carry in
        self.assertEqual((c.a, bool(c.p & C), bool(c.p & N)), (0x80, True, True))
        c = run([0x4A], a=0x03)                       # LSR A
        self.assertEqual((c.a, bool(c.p & C)), (0x01, True))

    def test_inc_wraps_and_rmw_writes_twice(self):
        cpu = make([0xEE, 0x00, 0x03])                # INC $0300
        cpu.mem.m[0x300] = 0xFF
        writes = []
        orig = cpu.mem.write
        cpu.mem.write = lambda a, v: (writes.append((a, v)), orig(a, v))
        cpu.step()
        self.assertEqual(cpu.mem.m[0x300], 0)
        self.assertTrue(cpu.p & Z)
        self.assertEqual(writes, [(0x300, 0xFF), (0x300, 0x00)])   # old value is written back first (NMOS)

    def test_stack(self):
        c = run([0x48, 0xA9, 0x00, 0x68], steps=3, a=0x42)            # PHA, LDA #0, PLA
        self.assertEqual((c.a, c.sp), (0x42, 0xFD))
        c = run([0x08], p=0x20 | C)                                   # PHP pushes with B and bit 5 set
        self.assertEqual(c.mem.m[0x1FD], 0x20 | B | C)
        c = run([0x28], p=0x20, sp=0xFC)                              # PLP ignores B, keeps bit 5
        c = make([0x28], sp=0xFC); c.mem.m[0x1FD] = 0xFF; c.step()
        self.assertEqual(c.p, 0xFF & ~B)

    def test_jsr_rts(self):
        cpu = make([0x20, 0x00, 0x03]); cpu.mem.m[0x300] = 0x60
        cpu.step()
        self.assertEqual((cpu.pc, cpu.sp), (0x300, 0xFB))
        self.assertEqual((cpu.mem.m[0x1FD], cpu.mem.m[0x1FC]), (0x02, 0x02))  # high byte first; address of last operand byte
        cpu.step()
        self.assertEqual((cpu.pc, cpu.sp), (0x203, 0xFD))

    def test_jmp_indirect_page_wrap_bug(self):
        cpu = make([0x6C, 0xFF, 0x02])
        cpu.mem.m[0x2FF] = 0x34; cpu.mem.m[0x200 + 0] = 0x6C   # low byte at $02FF, high byte wraps to $0200
        cpu.mem.m[0x300] = 0x99                                 # a bug-free CPU would read this
        cpu.step()
        self.assertEqual(cpu.pc, 0x6C34)                       # high byte came from $0200 (= $6C)

    def test_brk_and_rti(self):
        cpu = make([0x00, 0xEA, 0xEA])
        cpu.mem.m[0xFFFE], cpu.mem.m[0xFFFF] = 0x00, 0x40
        cpu.mem.m[0x4000] = 0x40                                # RTI
        cpu.p = 0x20
        cpu.step()
        self.assertEqual(cpu.pc, 0x4000)
        self.assertEqual((cpu.mem.m[0x1FD], cpu.mem.m[0x1FC]), (0x02, 0x02))  # return = BRK + 2
        self.assertTrue(cpu.mem.m[0x1FB] & B)
        self.assertTrue(cpu.p & I)
        cpu.step()
        self.assertEqual(cpu.pc, 0x202)

    def test_zero_page_pointer_wrap(self):
        cpu = make([0xB1, 0xFF], y=0)                           # LDA ($FF),Y: high byte of pointer read from $00
        cpu.mem.m[0xFF], cpu.mem.m[0x00], cpu.mem.m[0x1234] = 0x34, 0x12, 0x77
        cpu.step()
        self.assertEqual(cpu.a, 0x77)

    def test_illegal_opcode_raises(self):
        with self.assertRaises(CPUError):
            run([0x02])


class Cycles(unittest.TestCase):
    def test_branch_cycles(self):
        self.assertEqual(make([0xD0, 0x02], p=0x20 | Z).step(), 2)                 # not taken
        self.assertEqual(make([0xD0, 0x02]).step(), 3)                             # taken, same page
        self.assertEqual(make([0xD0, 0x02], org=0x02FD).step(), 4)                 # taken, crosses page

    def test_indexed_read_page_cross(self):
        self.assertEqual(make([0xBD, 0xFF, 0x01], x=1).step(), 5)
        self.assertEqual(make([0xBD, 0x00, 0x01], x=1).step(), 4)
        self.assertEqual(make([0x9D, 0x00, 0x01], x=1).step(), 5)                  # STA abs,X never faster

    def test_misc_cycles(self):
        for prog, cyc in (([0xA9, 1], 2), ([0xA5, 0], 3), ([0xAD, 0, 3], 4), ([0xE6, 0], 5), ([0xEE, 0, 3], 6),
                          ([0x48], 3), ([0x68], 4), ([0x20, 0, 3], 6), ([0x4C, 0, 3], 3), ([0x6C, 0, 3], 5),
                          ([0xA1, 0], 6), ([0x91, 0], 6), ([0xB1, 0], 5), ([0x00], 7)):
            self.assertEqual(make(prog).step(), cyc, prog)

    def test_small_program_total_cycles(self):
        # copy 16 bytes: LDX #0 / LDA $0300,X / STA $0400,X / INX / CPX #16 / BNE / BRK
        prog = [0xA2, 0, 0xBD, 0, 3, 0x9D, 0, 4, 0xE8, 0xE0, 16, 0xD0, 0xF5, 0x00]
        cpu = make(prog)
        for i in range(16):
            cpu.mem.m[0x300 + i] = 0xA0 + i
        while cpu.mem.m[cpu.pc] != 0x00:
            cpu.step()
        self.assertEqual(bytes(cpu.mem.m[0x400:0x410]), bytes(range(0xA0, 0xB0)))
        self.assertEqual(cpu.cycles, 2 + 16 * 13 + 15 * 3 + 2)                    # = 257


class Arithmetic(unittest.TestCase):
    def test_known_adc_sbc(self):
        c = run([0x69, 0x50], a=0x50)                                              # 0x50+0x50
        self.assertEqual((c.a, flags(c)['V'], flags(c)['N'], flags(c)['C']), (0xA0, True, True, False))
        c = run([0x69, 0x01], a=0xFF)
        self.assertEqual((c.a, flags(c)['Z'], flags(c)['C']), (0, True, True))
        c = run([0xE9, 0xF0], a=0x50, p=0x20 | C)                                  # 0x50-0xF0
        self.assertEqual((c.a, flags(c)['V'], flags(c)['C']), (0x60, False, False))
        c = run([0xE9, 0xB0], a=0x50, p=0x20 | C)                                  # 0x50-0xB0
        self.assertEqual((c.a, flags(c)['V'], flags(c)['C']), (0xA0, True, False))

    def test_sbc_equals_adc_of_complement_exhaustive(self):
        for a in range(256):
            for m in range(256):
                for cin in (0, 1):
                    s = CPU(RAM()); s.a = a; s.p = 0x20 | cin; s.sbc(m)
                    t = CPU(RAM()); t.a = a; t.p = 0x20 | cin; t.adc(m ^ 0xFF)
                    self.assertEqual((s.a, s.p), (t.a, t.p), (a, m, cin))

    def test_decimal_adc_matches_reference_model(self):
        for x in range(100):
            for y in range(100):
                for cin in (0, 1):
                    cpu = CPU(RAM()); cpu.p = 0x20 | D | cin; cpu.a = (x // 10) << 4 | x % 10
                    cpu.adc((y // 10) << 4 | y % 10)
                    tot = x + y + cin
                    want = ((tot % 100) // 10) << 4 | (tot % 100) % 10
                    self.assertEqual((cpu.a, bool(cpu.p & C)), (want, tot > 99), (x, y, cin))

    def test_decimal_sbc_matches_reference_model(self):
        for x in range(100):
            for y in range(100):
                for cin in (0, 1):
                    cpu = CPU(RAM()); cpu.p = 0x20 | D | cin; cpu.a = (x // 10) << 4 | x % 10
                    cpu.sbc((y // 10) << 4 | y % 10)
                    diff = x - y - (1 - cin)
                    res = diff % 100
                    want = (res // 10) << 4 | res % 10
                    self.assertEqual((cpu.a, bool(cpu.p & C)), (want, diff >= 0), (x, y, cin))


if __name__ == '__main__':
    unittest.main()
