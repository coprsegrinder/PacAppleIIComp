import hashlib, os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from make_manifest import data_ranges, render, content_hash


class Manifest(unittest.TestCase):
    def test_ranges_are_the_gaps_between_code(self):
        code = {0x4000, 0x4001, 0x4002, 0x4005}
        self.assertEqual(data_ranges(code, 0x4000, 0x4008), [(0x4003, 2), (0x4006, 2)])

    def test_code_at_both_ends_and_all_code(self):
        self.assertEqual(data_ranges({0x4000, 0x4001}, 0x4000, 0x4002), [])
        self.assertEqual(data_ranges(set(), 0x4000, 0x4003), [(0x4000, 3)])

    def test_render_has_addresses_only(self):
        text = render([(0x4064, 224), (0x8DBE, 96)])
        self.assertIn('{0x4064, 224},', text)
        self.assertIn('{0x8DBE, 96},', text)
        self.assertIn('std::span<const Range> dataManifest()', text)

    def test_content_hash_concatenates_in_address_order(self):
        body = bytes(range(32))
        ranges = [(0x4010, 4), (0x4002, 3)]
        expect = hashlib.sha256(body[2:5] + body[16:20]).hexdigest().upper()
        self.assertEqual(content_hash(body, ranges), expect)


if __name__ == '__main__':
    unittest.main()
