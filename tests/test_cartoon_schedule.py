"""TASK-031c: which intermission follows which level. Runs the original scheduler $8155 with $55 = level and sees where it goes
(cartoon 1 = $81AE, 2 = $83D0, 3 = $8486, none = returns). Result: cartoon 1 after levels 2, 15, 28, ...; 2 after 5, 18, 31, ...; 3 after 9, 22, 35, ...
(level mod 13 = 2, 5, 9). Needs scratch/ (original data); about 30 seconds. Run: python -I tests/test_cartoon_schedule.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))


class Schedule(unittest.TestCase):
    def test_scheduler_matches_mod_13_rule_for_levels_1_to_255(self):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('scratch not present')
        from play import do_step
        from cov_close import factory
        mc = factory()
        for st in ('key:1', 'key:cr', '5_000_000'):
            do_step(mc, st, lambda *_: None)
        saved = bytes(mc.m[0:0x100]), mc.cpu.sp
        targets = {0x81AE: 1, 0x83D0: 2, 0x8486: 3}
        got = {}
        for level in range(1, 256):
            c = mc.cpu
            mc.m[0x55] = level
            c.sp = 0xF0
            mc.m[0x1F1], mc.m[0x1F2] = 0x26, 0x40          # RTS returns to $4027, where run() stops
            c.pc = 0x8155
            for _ in range(600):
                if c.pc in targets or c.pc == 0x4027:
                    break
                c.step()
            got[level] = targets.get(c.pc, 0)
        rule = {2: 1, 5: 2, 9: 3}
        for level in range(1, 256):
            self.assertEqual(got[level], rule.get(level % 13, 0), level)
        self.assertEqual([l for l in got if got[l] == 1][:4], [2, 15, 28, 41])
        self.assertEqual([l for l in got if got[l] == 3][:3], [9, 22, 35])


if __name__ == '__main__':
    unittest.main()
