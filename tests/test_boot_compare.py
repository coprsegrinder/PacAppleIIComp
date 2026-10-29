import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from boot_compare import sec, groups, compare_boot, used_sectors, deleted_entries, hidden_data


def blank():
    return bytearray(143360)


def put(img, t, s, data, off=0):
    o = (t * 16 + s) * 256 + off
    img[o:o + len(data)] = bytes(data)


def disk_with_one_file():
    img = blank()
    put(img, 17, 0, [0, 17, 15])                 # VTOC: first catalog sector T17/S15
    entry = [3, 0, 0x04] + [0xC1] * 30 + [2, 0]  # T/S list at T3/S0, binary, 2 sectors
    put(img, 17, 15, [0, 0, 0], 0)               # next catalog sector: none
    put(img, 17, 15, entry, 11)
    put(img, 17, 15, [0xFF, 0, 0x04] + [0xC4] * 30 + [1, 0], 11 + 35)   # deleted entry (track marker $FF)
    put(img, 3, 0, [0, 0, 0] + [0] * 9 + [4, 0, 4, 1], 0)               # list: data at T4/S0 and T4/S1
    return img


class Helpers(unittest.TestCase):
    def test_groups(self):
        self.assertEqual(groups([5, 3, 4, 9]), [[3, 5], [9, 9]])

    def test_compare_boot_reports_only_differing_sectors(self):
        a, b = blank(), blank()
        put(a, 1, 3, [7], 10)
        self.assertEqual(compare_boot(a, b), {(1, 3): [10]})
        self.assertEqual(compare_boot(a, b, tracks=(0,)), {})

    def test_used_sectors_follow_catalog_lists_and_data(self):
        img = disk_with_one_file()
        u = used_sectors(img)
        for ts in [(0, 0), (2, 15), (17, 0), (17, 15), (3, 0), (4, 0), (4, 1)]:
            self.assertIn(ts, u)
        self.assertNotIn((5, 5), u)

    def test_deleted_entries(self):
        self.assertEqual(deleted_entries(disk_with_one_file()), ['D' * 30])

    def test_hidden_data_finds_nonzero_unowned_sector_only(self):
        img = disk_with_one_file()
        put(img, 4, 0, [1, 2, 3])                # owned data: must not be reported
        put(img, 20, 7, [9] * 5)                 # unowned, non-zero: must be reported
        self.assertEqual(hidden_data(img), [(20, 7, 5)])


if __name__ == '__main__':
    unittest.main()
