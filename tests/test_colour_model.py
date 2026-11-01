"""TASK-027e: the standard Apple II hi-res colour model (isolated pixel: violet/green with palette bit 0, blue/orange with palette bit 1,
even/odd x; neighbours: white) agrees with the colours AppleWin 1.32.1.0 actually shows for one real gameplay screen
(scratch/aw_play.png + scratch/gp.bin, window-only screenshot and memory dump of the same Esc-paused state). Skipped if the files are missing.
Run: python -I tests/test_colour_model.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))


class ColourModel(unittest.TestCase):
    def test_standard_parity_agrees_and_flipped_parity_does_not(self):
        if not (os.path.exists('scratch/aw_play.png') and os.path.exists('scratch/gp.bin')):
            self.skipTest('AppleWin screenshot/dump not present')
        import colour_check
        r = colour_check.main()
        total, agree = r[False]
        self.assertGreater(total, 2000)
        self.assertGreaterEqual(agree / total, 0.95)
        self.assertLess(r[True][1] / r[True][0], 0.05)


if __name__ == '__main__':
    unittest.main()
