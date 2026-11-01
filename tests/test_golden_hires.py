"""TASK-027c: the oracle's hi-res page at 22 checkpoints equals the stored SHA-256 list (tests/golden/hires_hashes.json), i.e. the golden
scenario is deterministic. The C++ hires layer will be compared against the same list. Replays the scenario (about 4 minutes).
Needs scratch/ (original data). Run: python -I tests/test_golden_hires.py"""
import json, os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))


class GoldenHires(unittest.TestCase):
    def test_replay_equals_stored_hashes(self):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('scratch not present')
        import dump_hires
        stored = json.load(open('tests/golden/hires_hashes.json'))
        got = dump_hires.run_golden()
        self.assertEqual([(g['label'], g['sha256']) for g in got], [(s['label'], s['sha256']) for s in stored])
        self.assertEqual(len(stored), 22)
        self.assertEqual(len({s['sha256'] for s in stored}), 22)           # every checkpoint is a different picture


if __name__ == '__main__':
    unittest.main()
