"""TASK-027d: the text printer model (tools/text_model.py) reproduces the oracle's hi-res page for every string the attract screens print
(page before the call + model == page after the call). Needs scratch/ (original data); about 1 minute. Run: python -I tests/test_text_model.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))


class TextModel(unittest.TestCase):
    def test_every_printed_string_matches(self):
        if not os.path.exists('scratch/extract/PAC-MAN.1000.bin'):
            self.skipTest('scratch not present')
        import text_model
        from apple2 import NeedKey
        from play import do_step
        from cov_close import factory
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        mc = factory()
        pending, results = [], []
        step = mc.cpu.step

        def hook():
            pc, c, m = mc.cpu.pc, mc.cpu, mc.m
            if pending and pc == pending[0][0]:
                _, rec, before = pending.pop(0)
                results.append((rec, before, bytes(m[0x2000:0x4000])))
            if pc == 0x42D0 and not pending:
                ret = ((m[0x100 + ((c.sp + 2) & 0xFF)] << 8 | m[0x100 + ((c.sp + 1) & 0xFF)]) + 1) & 0xFFFF
                pending.append((ret, ((m[0x14] | m[0x15] << 8) + c.y) & 0xFFFF, bytes(m[0x2000:0x4000])))
            return step()
        mc.cpu.step = hook
        for st in ('key:1', 'key:cr'):
            do_step(mc, st, lambda *_: None)
        for _ in range(12):
            try:
                mc.run(2_000_000)
            except NeedKey:
                mc.push_key(13)
        self.assertGreaterEqual(len(results), 10)
        for rec, before, after in results:
            page = bytearray(before)
            text_model.draw_string(page, body, rec)
            self.assertEqual(bytes(page), after, hex(rec))
            self.assertNotEqual(before, after, hex(rec))               # the string really drew something
        self.assertGreaterEqual(len({r[0] for r in results}), 10)


if __name__ == '__main__':
    unittest.main()
