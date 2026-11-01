"""TASK-031f: (1) a key pressed during a cartoon is discarded: with the keyboard latch set when the cartoon returns, the first keyboard access is the
strobe clear at $565C (start of the next level) and the game state 400 ticks later equals a control run without the key; (2) in a 2-player game each player
gets cartoon 1 after their own level 2. Needs scratch/ (original data); about 7 minutes. Run: python -I tests/test_cartoon_flow.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))


class CartoonFlow(unittest.TestCase):
    def test_key_during_cartoon_is_discarded_and_each_player_gets_a_cartoon(self):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('scratch not present')
        import probe_cartoon_flow as p
        import time
        res = {}
        for press in (False, True):
            p.START = time.monotonic()
            res[press] = p.run_key(press)
        (st0, log0), (st1, log1) = res[False], res[True]
        self.assertIsNotNone(st0['after'])
        self.assertEqual(st0['cartoon_at'], st1['cartoon_at'])
        self.assertEqual(st1['kbd_at_return'][0] & 0x80, 0x80)                 # the key is latched when the cartoon returns
        self.assertEqual(log1[0][:2], ('$C010', 'pc $565C'))                    # the first keyboard access: strobe clear at the next level's set-up
        self.assertEqual(st0['after'], st1['after'])                           # nothing of the key is left 400 ticks later
        p.START = time.monotonic()
        seen = p.probe_two()
        starts = [s for s in seen if 'CARTOON STARTS' in s[1]]
        self.assertEqual([(s[1].split('$50 = ')[1], s[2]) for s in starts][:2], [('1', 'level $55 = 2'), ('2', 'level $55 = 2')])


if __name__ == '__main__':
    unittest.main()
