"""Tests for tools/trace_pacman.py (needs scratch/extract from the original disk; run: python -I tests/test_pacman_tools.py)."""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import trace_pacman, trace_speed, trace_eatscore


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


class PacmanRules(unittest.TestCase):
    def test_step_delay_rule_matches_code(self):
        rows = trace_speed.run(3, 20_000_000)
        self.assertGreater(len(rows), 100)
        self.assertTrue(all(r[0] == r[1] for r in rows))
        self.assertEqual({r[0] for r in rows} - {9, 11}, set())

    def test_ghost_eaten_scores_200_and_bonus_life_at_10000(self):
        log, bonus, lives, e8 = trace_eatscore.run(7, 100_000_000, want=1)
        self.assertEqual((log[0]['score_after'] - log[0]['score_before'], log[0]['step'], log[0]['step_after']), (0x200, 2, 4))
        self.assertTrue(any(l == 4 and sc >= 0x10000 for l, sc in lives))   # $58 = $04: 4 lives, bonus flag consumed


if __name__ == '__main__':
    unittest.main()
