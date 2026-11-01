"""TASK-027a: the set of instructions that write to hi-res page 1 in the recorded scenario, and the video switches used.
Needs scratch/screen_writers.json (made by tools/trace_screen_writers.py, about 2.5 minutes); skipped if missing. Run: python -I tests/test_screen_writers.py"""
import json, os, unittest

HIRES1 = [0x55B6, 0x5918, 0x5D09, 0x6183, 0x618C, 0x619C, 0x61A5, 0x6955, 0x695E, 0x6985, 0x69BE, 0x6A51, 0x6F02, 0x6F50, 0x7830, 0x783A]


class ScreenWriters(unittest.TestCase):
    def setUp(self):
        if not os.path.exists('scratch/screen_writers.json'):
            self.skipTest('scratch/screen_writers.json not present')
        self.d = json.load(open('scratch/screen_writers.json'))

    def test_hires_page1_has_exactly_these_sixteen_writers(self):
        self.assertEqual(sorted(w[1] for w in self.d['writers'] if w[0] == 'hires1'), HIRES1)

    def test_nothing_writes_text_page_1_and_the_only_page2_writes_are_program_patches_or_game_data(self):
        self.assertEqual([w for w in self.d['writers'] if w[0] == 'text1'], [])
        for w in self.d['writers']:
            if w[0] == 'hires2':                          # $4000-$5FFF holds program and its self-modified operands, not a picture
                self.assertTrue(w[3] >= 0x4000)

    def test_switches_used(self):
        used = sorted({s[0] for s in self.d['switches']})
        self.assertEqual(used, [0xC050, 0xC051, 0xC052, 0xC054, 0xC055, 0xC056, 0xC057])   # never $C053 (mixed mode)


if __name__ == '__main__':
    unittest.main()
