import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from save_clicks import notes


class Notes(unittest.TestCase):
    def test_two_pitches(self):
        clicks = [0, 100, 200, 300, 400] + [1000, 1200, 1400, 1600]
        self.assertEqual(notes(clicks), [(0, 5, 100), (1000, 4, 200)])

    def test_jitter_within_tolerance_stays_one_run(self):
        clicks = [0, 100, 205, 300, 395]
        self.assertEqual(notes(clicks), [(0, 5, 100)])

    def test_isolated_clicks_are_not_notes(self):
        self.assertEqual(notes([0, 5000, 5100, 20000]), [])

    def test_empty_and_single(self):
        self.assertEqual(notes([]), [])
        self.assertEqual(notes([7]), [])


if __name__ == '__main__':
    unittest.main()
