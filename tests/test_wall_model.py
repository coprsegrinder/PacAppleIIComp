"""TASK-027d: the wall model (tools/wall_model.py) reproduces the oracle's hi-res page right after the original's wall routine $5D53,
for the normal flag ($1D = 1, table $5F69) and for the forced flag ($1D = 0, table $5DBF; the code bytes that are never used in the real game,
docs/FORCED_CODE.md). Needs scratch/ (original data); each run replays the boot (about 1-2 minutes). Run: python -I tests/test_wall_model.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))


def oracle_page(force_zero):
    from apple2 import NeedKey
    from play import do_step
    from cov_close import factory
    mc = factory()
    seen = {}
    step = mc.cpu.step

    def hook():
        pc = mc.cpu.pc
        if pc == 0x5662 and 'page' not in seen:
            if force_zero:
                mc.m[0x1D] = 0
            seen['flag'] = mc.m[0x1D]
        if pc == 0x5665 and 'flag' in seen and 'page' not in seen:
            seen['page'] = bytes(mc.m[0x2000:0x4000])
        return step()
    mc.cpu.step = hook
    for st in ('key:1', 'key:cr'):
        do_step(mc, st, lambda *_: None)
    for _ in range(40):
        if 'page' in seen:
            break
        try:
            mc.run(2_000_000)
        except NeedKey:
            mc.push_key(13)
    return seen


class WallModel(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            raise unittest.SkipTest('scratch not present')
        cls.body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()

    def table(self, addr):
        return self.body[addr - 0x4000: addr - 0x4000 + 426]

    def test_normal_flag_matches_the_oracle(self):
        import wall_model
        seen = oracle_page(False)
        self.assertEqual(seen['flag'], 1)
        self.assertNotEqual(seen['page'], bytes([0x80]) * 0x2000)          # something was drawn
        self.assertEqual(wall_model.wall_page(self.table(0x5F69), 1), seen['page'])

    def test_forced_zero_flag_matches_the_oracle(self):
        import wall_model
        seen = oracle_page(True)
        self.assertEqual(seen['flag'], 0)
        self.assertEqual(wall_model.wall_page(self.table(0x5DBF), 0), seen['page'])

    def test_the_two_modes_really_differ(self):
        import wall_model
        a, b = wall_model.wall_page(self.table(0x5F69), 1), wall_model.wall_page(self.table(0x5DBF), 0)
        self.assertNotEqual(a, b)


if __name__ == '__main__':
    unittest.main()
