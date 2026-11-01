"""TASK-031c: the three intermission cartoons on the oracle (hidden menu, Ctrl-V Ctrl-C, C, 1-3).
* each cartoon's drawing-call chain, final hi-res page hash, cycle count and call counts equal tests/golden/cartoon_hashes.json (replay is deterministic);
* the drawing calls of all three cartoons equal the reference models in tools/cartoon_model.py call for call (562, 482 and 814 calls).
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
            if n in (1, 2, 3):
                orc = cartoon_model.oracle_calls(out['events'])
                self.assertEqual(len(orc), {1: 562, 2: 482, 3: 814}[n])
                self.assertEqual(orc, getattr(cartoon_model, 'cartoon%d' % n)(body), n)


if __name__ == '__main__':
    unittest.main()
