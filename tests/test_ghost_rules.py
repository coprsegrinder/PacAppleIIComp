"""Tests for tools/ghost_rules.py (needs scratch/extract from the original disk; run: python -I tests/test_ghost_rules.py)."""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import ghost_rules as gr


class Helpers(unittest.TestCase):
    def test_class_reps_cover_every_value_once(self):
        for b in ([], [0], [255], [5, 100, 255], [0, 1, 2], [34, 100, 101]):
            reps = gr.class_reps(b)
            self.assertEqual(sorted(set(reps)), reps)
            # every value maps to a representative, and values of one class map to the same one
            for v in range(256):
                self.assertIn(gr.class_of(v, b), reps)
            for v in b:
                self.assertEqual(gr.class_of(v, b), v)

    def test_breakpoints_add_abs_difference_thresholds(self):
        self.assertEqual(gr.breakpoints([34], 100, False), [34, 100])
        b = gr.breakpoints([], 100, True)
        for k in (0x1F, 0x20, 0x21):
            self.assertIn(100 - k, b)
            self.assertIn(100 + k, b)


class Routines(unittest.TestCase):
    def test_static_scan_all_48_read_only_known_bytes(self):
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        words = gr.routine_words(body)
        reads = set()
        for r in range(48):
            cx, cy, zp, absdiff, bad = gr.closure_scan(body, words, r)
            self.assertEqual(bad, [], r)
            self.assertTrue(zp <= gr.READS_OK | {0x72}, (r, zp))
            reads |= zp
        self.assertEqual(reads, {0x62, 0x69, 0x70, 0x71, 0x72})

    def test_tables_equal_oracle_on_random_inputs(self):
        info, summary = gr.build_tables(nsamples=150, only={0, 4, 18, 24, 38, 45})
        self.assertEqual(summary['mismatches'], 0)
        self.assertEqual(summary['no_return'], 0)
        self.assertGreater(summary['checked_points'], 500)
        self.assertEqual(info[0]['depends_on'], ['dir'])
        self.assertIn('$62', info[24]['depends_on'])
        self.assertIn('$69', info[18]['depends_on'])
        self.assertNotIn('$62', info[45]['depends_on'])


if __name__ == '__main__':
    unittest.main()
