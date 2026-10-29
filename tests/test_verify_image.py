import hashlib, os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from verify_image import verify, EXACT, PROGRAM, FAIL, UNREADABLE


def put(img, t, s, data, off=0):
    o = (t * 16 + s) * 256 + off
    img[o:o + len(data)] = bytes(data)


def make_disk(body, name='PAC-MAN', ftype=4, addr=0x1000, volume=254, dos_marker=0):
    """Synthetic DOS 3.3 disk: VTOC, one catalog sector, one file whose T/S list is at T3/S0 and data at T4..."""
    img = bytearray(143360)
    put(img, 17, 0, [4, 17, 15, 3], 0)
    img[(17 * 16) * 256 + 0x06] = volume
    img[(17 * 16) * 256 + 0x34] = 35
    img[(17 * 16) * 256 + 0x35] = 16
    put(img, 0, 5, [dos_marker])                          # stands in for DOS boot-track content
    data = bytes([addr & 0xFF, addr >> 8, len(body) & 0xFF, len(body) >> 8]) + body
    nsec = (len(data) + 255) // 256
    entry = [3, 0, ftype] + [ord(c) | 0x80 for c in name.ljust(30)] + [nsec & 0xFF, nsec >> 8]
    put(img, 17, 15, [0, 0, 0], 0)
    put(img, 17, 15, entry, 11)
    lst = []
    for i in range(nsec):
        t, s = 5 + i // 16, i % 16
        lst += [t, s]
        put(img, t, s, data[i * 256:(i + 1) * 256])
    put(img, 3, 0, lst, 12)
    return img


BODY = bytes((i * 7) & 0xFF for i in range(1000))
EXP = {'image_sha256': hashlib.sha256(make_disk(BODY)).hexdigest().upper(), 'file_name': 'PAC-MAN', 'file_type': 4,
       'load_address': 0x1000, 'length': len(BODY), 'body_sha256': hashlib.sha256(BODY).hexdigest().upper()}


class Verify(unittest.TestCase):
    def test_exact_image(self):
        self.assertEqual(verify(bytes(make_disk(BODY)), EXP)[0], EXACT)

    def test_different_dos_tracks_and_volume_still_verify_the_program(self):
        img = make_disk(BODY, volume=1, dos_marker=0x99)
        self.assertEqual(verify(bytes(img), EXP)[0], PROGRAM)

    def test_one_changed_program_byte_fails(self):
        body = bytearray(BODY)
        body[500] ^= 1
        self.assertEqual(verify(bytes(make_disk(bytes(body))), EXP)[0], FAIL)

    def test_wrong_file_name_type_or_load_address_fail(self):
        for kw in ({'name': 'OTHER'}, {'ftype': 2}, {'addr': 0x2000}):
            self.assertEqual(verify(bytes(make_disk(BODY, **kw)), EXP)[0], FAIL, kw)

    def test_wrong_size_and_non_dos_images(self):
        self.assertEqual(verify(b'\x00' * 100, EXP)[0], UNREADABLE)
        self.assertEqual(verify(bytes(143360), EXP)[0], FAIL)

    def test_damaged_catalog_does_not_raise(self):
        img = make_disk(BODY)
        put(img, 3, 0, [0] * 5, 0)
        put(img, 17, 15, [99, 99, 99], 0)              # next catalog sector points outside the disk
        status, _ = verify(bytes(img), EXP)
        self.assertIn(status, (FAIL, PROGRAM))


if __name__ == '__main__':
    unittest.main()
