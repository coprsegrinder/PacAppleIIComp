import json, os, random, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from cov_union import exec_set, to_runs, compare
from autoplay import play_one
from apple2 import Machine


class Union(unittest.TestCase):
    def test_exec_set_clips_to_game_area_and_to_runs_merges(self):
        p = os.path.join(tempfile.mkdtemp(), 'c.json')
        with open(p, 'w') as f:
            json.dump({'exec': [[0x1000, 0x1010], [0x4000, 0x4002], [0xA9FE, 0xAA05]]}, f)
        s = exec_set(p)
        self.assertEqual(sorted(s), [0x4000, 0x4001, 0x4002, 0xA9FE, 0xA9FF])
        self.assertEqual(to_runs(s), [[0x4000, 0x4002], [0xA9FE, 0xA9FF]])

    def test_compare_reports_contradictions_and_gaps(self):
        r = compare({1, 2, 9}, {1, 2, 3})
        self.assertEqual((r['executed_and_static'], r['executed_not_static'], r['static_not_executed']), (2, [9], [3]))


class AutoPlay(unittest.TestCase):
    def test_play_one_stops_when_demo_flag_set_and_drives_paddles(self):
        mc = Machine()
        mc.m[0xE8] = 1
        self.assertEqual(play_one(mc, 2, random.Random(1), 1_000_000), (0, True))
        mc.m[0xE8] = 0
        mc.m[0x300:0x303] = bytes([0x4C, 0x00, 0x03])            # JMP $0300 (endless loop)
        mc.cpu.pc = 0x300
        done, over = play_one(mc, 2, random.Random(1), 450_000, step=150_000)
        self.assertEqual((done, over), (450_000, False))
        self.assertIn(mc.paddles[0], (0, 127, 255))


if __name__ == '__main__':
    unittest.main()
