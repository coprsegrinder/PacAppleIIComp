"""Tests for tools/sound_model.py and tools/trace_sound.py (needs scratch/extract; run: python -I tests/test_sound_tools.py)."""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import sound_model as sm
import trace_sound
from play import make_machine


class Model(unittest.TestCase):
    def test_model_formulas_on_synthetic_lists(self):
        self.assertEqual(sm.model_clicks('A', [10], None, 0x1000), [0, 101, 202, 303])           # 9*10+11
        self.assertEqual(sm.model_clicks('A', [10, 20], None, 0x1000)[3:5], [303, 303 + 9 * 20 + 44])
        # page crossing adds one cycle only to the note whose list read crosses the page
        self.assertEqual(sm.model_clicks('A', [10, 10], None, 0x10FF)[4] - sm.model_clicks('A', [10, 10], None, 0x1000)[4], 1)
        self.assertEqual(sm.model_clicks('B', [(10, 2), (20, 1)], 1000, 0)[:3], [0, 321, 31 * 20 + 1000 + 321])

    def test_reader_stops_at_zero(self):
        body = bytes([5, 6, 0, 9]) + bytes(8)
        self.assertEqual(sm.read_list(body, 0x4000, 'A'), [5, 6])
        self.assertEqual(sm.read_list(bytes([5, 2, 7, 1, 0, 0]), 0x4000, 'B'), [(5, 2), (7, 1)])

    def test_model_can_fail(self):
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        mc = make_machine()
        notes = sm.read_list(body, 0x7C4C, 'A')
        got = sm.oracle_clicks(mc, 'A', 0x7C4C)
        wrong = sm.model_clicks('A', notes[:-1] + [notes[-1] + 1], None, 0x7C4C)
        self.assertNotEqual(got, wrong)


class Oracle(unittest.TestCase):
    def test_all_sound_lists_equal_model(self):
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        mc = make_machine()
        for name, how, eng, addr in sm.SOUNDS:
            notes = sm.read_list(body, addr, eng)
            got = sm.oracle_clicks(mc, eng, addr)
            if eng == 'C':
                cb = got[4] - got[3] - 9 * 32
            elif eng == 'B':
                cb = (got[notes[0][1]] - got[notes[0][1] - 1]) - 31 * notes[1][0]
            else:
                cb = None
            self.assertEqual(got, sm.model_clicks(eng, notes, cb, addr), name)

    def test_play_clicks_only_from_engines_and_dot_sound_matches(self):
        res, tabs, stray, n = trace_sound.run(1, 12_000_000)
        self.assertGreater(n, 100)
        self.assertEqual(stray, [])
        self.assertTrue(res['dot eaten'] and all(res['dot eaten']))


if __name__ == '__main__':
    unittest.main()
