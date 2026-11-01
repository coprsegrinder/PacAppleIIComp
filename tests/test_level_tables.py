"""Tests for tools/level_tables.py and tools/trace_fruit.py (needs scratch/extract; run: python -I tests/test_level_tables.py)."""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import level_tables as lt
import trace_fruit as tf
from play import make_machine


class Tables(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()

    def test_read_helpers(self):
        self.assertEqual(lt.read(bytes([1, 2, 3, 4]) + bytes(8), 0x4000, 3), [1, 2, 3])
        self.assertEqual(lt.read(bytes([1, 2, 3, 4]), 0x4000, 2, word=True), [0x201, 0x403])

    def test_all_level_one_values_match_earlier_findings(self):
        t = {n: lt.read(self.body, a, c, n in ('eaten_timer_extension', 'frightened_time')) for n, a, c, *_ in lt.TABLES}
        self.assertEqual(t['pacman_delay'][1], 9)                     # PACMAN_RULES.md
        self.assertEqual((t['house_ghost2'][1], t['house_ghost3'][1], t['house_ghost4'][1]), (112, 48, 144))   # MOVEMENT.md
        self.assertEqual(t['frightened_time'][1], 352)                # GHOST_MODES.md
        self.assertEqual(t['fruit_score_hundreds'][1], 1)

    def test_fruit_schedule_shape(self):
        f = lt.fruit_schedule(self.body)
        self.assertEqual(len(f), 27)
        self.assertEqual([r['fruit_index'] for r in f[1:5]], [0, 1, 2, 2])

    def test_every_table_is_clamped_inside_its_extent(self):
        for name, addr, n, clamp, *_ in lt.TABLES:
            self.assertLessEqual(clamp + 1, n, name)


class Oracle(unittest.TestCase):
    def test_fruit_setup_and_value_equal_tables_for_levels_1_to_30(self):
        body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
        rows = tf.per_level()
        f = lt.fruit_schedule(body)
        vals = lt.read(body, 0x6CD4, 16)
        for lv in range(1, 31):
            e = f[min(lv, 26)]
            self.assertEqual(rows[lv]['sprite_ptr'], e['sprite_ptr'], lv)
            self.assertEqual(rows[lv]['lifetime'], e['lifetime_ticks'], lv)
            self.assertEqual(rows[lv]['score_hundreds'], '%02X' % vals[min(lv, 15)], lv)
            self.assertEqual(rows[lv]['clicks'], 100, lv)             # the fruit-eaten sound is the $7C61 list
            self.assertEqual(rows[lv]['next_57'], 96, lv)

    def test_fruit_lives_exactly_lifetime_ticks(self):
        mc = make_machine()
        m = mc.m
        m[0x55], m[0x57], m[0xE7], m[0x59], m[0xE8] = 1, 0, 0, 5, 0
        tf.call(mc, 0x6A7C)
        life = m[0x5A] | m[0x5B] << 8
        m[0x59] = 1
        n = 0
        while m[0xE7] == 1 and n < 1000:
            tf.call(mc, 0x6A7C)
            n += 1
        self.assertEqual((n, m[0xE7]), (life, 0))


if __name__ == '__main__':
    unittest.main()
