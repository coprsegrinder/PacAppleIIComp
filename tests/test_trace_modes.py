import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine
import trace_modes as tm
import zp_refs as zr
from recdis import analyse


class Inject(unittest.TestCase):
    def test_simulated_jsr_runs_the_routine_and_returns_to_the_same_place(self):
        mc = Machine()
        mc.m[0x300] = 0xEA                                         # NOP at the injection point
        mc.m[0x400:0x403] = bytes([0xA9, 0x07, 0x60])               # LDA #7 ; RTS
        mc.cpu.pc = 0x300
        tm.simulate_jsr(mc, 0x400)
        mc.run(10, stop_pc=0x300)
        self.assertEqual((mc.cpu.a, mc.cpu.pc, mc.cpu.sp), (7, 0x300, 0xFD))


class Watch(unittest.TestCase):
    def test_changes_of_watched_variables_are_logged_with_the_writer(self):
        mc = Machine()
        mc.m[0x300:0x306] = bytes([0xA9, 0x01, 0x85, 0xE9, 0xEA, 0xEA])      # LDA #1 ; STA $E9
        mc.cpu.pc = 0x300
        events, counters = tm.install(mc)
        mc.run(2)
        self.assertEqual(len(events), 1)
        e = events[0]
        self.assertEqual((e['var'], e['old'], e['new'], e['pc']), ('E9', 0, 1, 0x302))

    def test_entries_to_the_three_counted_routines_are_counted(self):
        mc = Machine()
        for a in (0x808C, 0x8763, 0x8772):
            mc.m[a] = 0xEA
        events, counters = tm.install(mc)
        for a in (0x808C, 0x808C, 0x8763, 0x8772, 0x8772, 0x8772):
            mc.cpu.pc = a
            mc.cpu.step()
        self.assertEqual(counters, {'mode_ticks': 2, 'ghost_steps': 1, 'fright_ticks': 3})

    def test_ghost_flags_read_the_four_records(self):
        mc = Machine()
        for g in range(4):
            mc.m[0x84 + 20 * g + 8] = g          # $78 of ghost g+1
            mc.m[0x84 + 20 * g + 9] = 10 + g     # $79
        self.assertEqual(tm.ghost_flags(mc), [(0, 10), (1, 11), (2, 12), (3, 13)])


class ZeroPageReferences(unittest.TestCase):
    def test_reads_writes_modifies_and_pointers_are_told_apart(self):
        # $4000: LDA $78 ; STA $79 ; INC $7A ; LDA ($7B),Y ; RTS
        body = bytes([0xA5, 0x78, 0x85, 0x79, 0xE6, 0x7A, 0xB1, 0x7B, 0x60])
        code, *_ = analyse(body, 0x4000, [0x4000])
        refs = zr.references(body, code, [0x78, 0x79, 0x7A, 0x7B, 0x7C])
        self.assertEqual(refs[0x78], [(0x4000, 'LDA', 'read')])
        self.assertEqual(refs[0x79], [(0x4002, 'STA', 'write')])
        self.assertEqual(refs[0x7A], [(0x4004, 'INC', 'modify')])
        self.assertEqual(refs[0x7B], [(0x4006, 'LDA', 'pointer')])
        self.assertEqual(refs[0x7C], [])


if __name__ == '__main__':
    unittest.main()
