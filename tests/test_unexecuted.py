"""Test for TASK-021: the union of the stored coverage files leaves nothing (100%) documented in docs/UNEXECUTED_CODE.md.
Needs scratch/extract and the scratch/cov*.json files made by tools/cov_close*.py and the earlier tools (skipped if they are missing).
Run: python -I tests/test_unexecuted.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import cov_union as cu
from recdis import analyse
from static_plus import table_targets, bytes_of

FILES = ['cov', 'cov_auto1', 'cov_auto2', 'cov_cartoons', 'cov_custom', 'cov_play', 'cov_rank', 'cov_close', 'cov_close2', 'cov_close3', 'cov_close5_C', 'cov_close5_D', 'cov_close5_E', 'cov_close4_A', 'cov_close4_B', 'cov_close5_6']
DOCUMENTED = []


class Unexecuted(unittest.TestCase):
    def test_remaining_runs_equal_the_documented_ones(self):
        paths = ['scratch/%s.json' % f for f in FILES]
        if not all(os.path.exists(p) for p in paths) or not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('coverage files not present')
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        code, *_ = analyse(body, cu.LO, [0x4027] + table_targets(body))
        static = bytes_of(code)
        total = set()
        for p in paths:
            total |= cu.exec_set(p)
        r = cu.compare(total, static)
        self.assertEqual(r['executed_not_static'], [])                      # the static map has no contradiction
        left = ['%04X-%04X' % (a, b) for a, b in cu.to_runs(r['static_not_executed'])]
        self.assertEqual(left, DOCUMENTED)
        self.assertEqual(len(r['static_not_executed']), 0)
        self.assertEqual(r['static'], 10456)

    def test_to_runs(self):
        self.assertEqual(cu.to_runs({5, 6, 7, 10}), [[5, 7], [10, 10]])


if __name__ == '__main__':
    unittest.main()
