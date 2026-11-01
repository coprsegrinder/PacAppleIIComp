"""TASK-027d: the data tables the drawing routines read lie entirely outside the static code map, i.e. inside the data pack
(the pack holds every byte that is not code, docs/DATAPACK_SPEC.md). Needs scratch/extract. Run: python -I tests/test_graphics_tables.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))

TABLES = {
    'font glyphs (33 x 9 bytes)': (0x4553, 33 * 9),
    'wall table $1D=0 (72 + 70 triples)': (0x5DBF, 426),
    'wall table $1D<>0 (72 + 70 triples)': (0x5F69, 426),
    'dot image shifts': (0x61A8, 14),
    'energizer tile shifts (7 x 16)': (0x61CA, 112),
    'maze dot map (28 columns x 32)': (0x623A, 28 * 32),
    'mask table A (7 x 4)': (0x69CD, 28),
    'mask table B (7 x 4)': (0x6A04, 28),
    'fruit-by-level pointers': (0x6AFE, 4 * 27),
    'fruit images (8 x 36)': (0x6B6E, 8 * 36),
    'cartoon big sprite frames (3 x 80)': (0x82E0, 3 * 80),
    'cartoon frame pointers': (0x82D8, 8),
    'hi-res row tables': (0x5B67, 192),
    'hi-res row tables (high)': (0x5C27, 192),
    'pixel column table': (0x5A58, 256),
    'pixel shift table': (0x5958, 256),
}


class GraphicsTables(unittest.TestCase):
    def test_tables_do_not_overlap_the_code_map(self):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('scratch not present')
        from recdis import analyse
        from static_plus import table_targets, bytes_of
        import cov_union as cu
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        code, *_ = analyse(body, cu.LO, [0x4027] + table_targets(body))
        static = bytes_of(code)
        self.assertEqual(len(static), 10456)
        bad = {n: sorted(a for a in range(s, s + k) if a in static)[:4] for n, (s, k) in TABLES.items()}
        self.assertEqual({n: v for n, v in bad.items() if v}, {})


if __name__ == '__main__':
    unittest.main()
