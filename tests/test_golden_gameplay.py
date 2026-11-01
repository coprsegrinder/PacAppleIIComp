"""TASK-034a: golden gameplay traces (tests/golden/gameplay_traces.json). The stored file must be internally sensible, and the oracle replay
of every scenario (inputs and pokes taken from the file, keyed by tick) must reproduce every checkpoint, sample, event count and the final hash.
The replay takes about 4 minutes and needs scratch/. Run: python -I tests/test_golden_gameplay.py"""
import json, os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))

STORED = 'tests/golden/gameplay_traces.json'


class GoldenGameplay(unittest.TestCase):
    def test_stored_file_covers_the_interesting_events(self):
        d = json.load(open(STORED))
        ev = {n: s['events'] for n, s in d['scenarios'].items()}
        self.assertEqual(sorted(ev), ['A', 'B', 'C', 'D'])
        self.assertGreaterEqual(ev['C'].get('level_complete', 0), 2)                    # two level ends (one passes the level-2 cartoon)
        self.assertGreater(ev['D']['collision_resolved'], ev['D']['pacman_caught'])     # ghosts were eaten, not only catches
        self.assertGreaterEqual(ev['D']['energizer_eaten'], 5)
        for n, s in d['scenarios'].items():
            self.assertEqual(s['events']['pacman_caught'], 3, n)                        # every scenario ends by losing the three lives
            self.assertEqual(s['checkpoints'][0][0], 0)
            self.assertEqual(len(s['final']), 64)
        self.assertEqual(len({s['final'] for s in d['scenarios'].values()}), 4)

    def test_replay_reproduces_every_scenario(self):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('scratch not present')
        import golden_trace
        self.assertEqual(golden_trace.check(json.load(open(STORED))), [])


if __name__ == '__main__':
    unittest.main()
