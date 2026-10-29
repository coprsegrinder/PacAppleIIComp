import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from compare_dump import to_runs, REGIONS


class Helpers(unittest.TestCase):
    def test_to_runs_merges_adjacent_and_sorts(self):
        self.assertEqual(to_runs([5, 3, 4, 9]), [[3, 5], [9, 9]])
        self.assertEqual(to_runs([]), [])

    def test_regions_tile_the_dump_without_gaps(self):
        edges = [(lo, hi) for _, lo, hi in REGIONS]
        self.assertEqual(edges[0][0], 0)
        self.assertEqual(edges[-1][1], 0xC000)
        for (_, h1), (l2, _) in zip(edges, edges[1:]):
            self.assertEqual(h1, l2)


class Scenarios(unittest.TestCase):
    def test_parse_scenario_labels(self):
        from compare_dump import parse_scenario
        self.assertEqual(parse_scenario('3_000_000')[0], '3000000 attract instructions')
        self.assertEqual(parse_scenario('play:3_000_000:3_500_000')[0], 'play after 3_000_000 attract + 3_500_000 play instructions')
        self.assertTrue(callable(parse_scenario('play:1:2')[1]))


if __name__ == '__main__':
    unittest.main()
