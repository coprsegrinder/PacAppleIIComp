"""TASK-031c: the three intermission cartoons on the oracle (hidden menu, Ctrl-V Ctrl-C, C, 1-3).
* each cartoon's drawing-call chain, final hi-res page hash, cycle count and call counts equal tests/golden/cartoon_hashes.json (replay is deterministic);
* cartoon 1's drawing calls equal the reference model tools/cartoon_model.py call for call (562 calls).
Needs scratch/ (original data); about 5 minutes. Run: python -I tests/test_cartoons.py"""
import json, os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))


class Cartoons(unittest.TestCase):
    def test_golden_and_model(self):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('scratch not present')
        import trace_cartoon, cartoon_model
        gold = json.load(open('tests/golden/cartoon_hashes.json'))
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        for n in (1, 2, 3):
            trace_cartoon.run_one(n)
            out = trace_cartoon.run_one.last
            g = gold[str(n)]
            self.assertEqual((out['draw_chain'], out['final_page_sha256'], out['cycles'], out['clicks']), (g['draw_chain'], g['final_page_sha256'], g['cycles'], g['clicks']), n)
            self.assertEqual(len(out['events']), g['events'])
            if n == 1:
                orc = []
                for e in out['events']:
                    k = e[1]
                    if k in ('page_clear', 'hires_on'):
                        orc.append((k,))
                    elif k == 'masked_clear':
                        orc.append((k, e[14], e[5]))
                    elif k == 'dispatch':
                        orc.append((k, e[2] | e[3] << 8, e[14], e[5]))
                    elif k == 'big_sprite':
                        orc.append((k, e[8], e[9], e[12]))
                self.assertEqual(len(orc), 562)
                self.assertEqual(orc, cartoon_model.cartoon1(body))


if __name__ == '__main__':
    unittest.main()
