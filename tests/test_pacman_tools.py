"""Tests for tools/trace_pacman.py (needs scratch/extract from the original disk; run: python -I tests/test_pacman_tools.py)."""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import trace_pacman


class PacmanTrace(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ev = trace_pacman.run(1, 1, 40_000_000, quiet=True)

    def test_dot_gives_10_points_and_one_dot_less(self):
        dots = [e for e in self.ev if e.get('mark') == 'dot_eaten']
        self.assertGreater(len(dots), 10)
        lo = [e for e in self.ev if e.get('var') == 'score_lo']
        self.assertTrue(all(e['new'] - e['old'] == 0x10 for e in lo[:9]))  # BCD 10 points per dot, before the first carry
        left = [e for e in self.ev if e.get('var') == 'dots_left']
        self.assertTrue(all(e['new'] == e['old'] - 1 for e in left))
        self.assertEqual(left[0]['old'], 248)

    def test_catch_costs_a_life(self):
        caught = [e for e in self.ev if e.get('mark') == 'pacman_caught']
        lives = [e for e in self.ev if e.get('var') == 'lives' and e['new'] == e['old'] - 1]
        self.assertGreaterEqual(len(caught), 2)
        self.assertGreaterEqual(len(lives), 2)
        self.assertEqual(lives[0]['old'], 0x83)


if __name__ == '__main__':
    unittest.main()
