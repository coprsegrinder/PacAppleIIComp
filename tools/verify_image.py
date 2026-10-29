"""Reference implementation of the disk-image verification rule (TASK-009; see docs/VERIFIER_SPEC.md).
Usage: python -I tools/verify_image.py <image.dsk>      exit code 0 = verified (exact image or game program), 1 = not verified, 2 = unreadable
Read-only. The expected hashes below come only from the image supplied by the user on 2026-10-29; no public source confirms them (docs/RESEARCH_007.md).
"""
import hashlib, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import dsk

IMAGE_SIZE = 143360
EXPECTED = {
    'image_sha256': 'F9E19375B15852B91776776A95755DD3F57E2B20A517AA99ED8603413E0FE805',
    'file_name': 'PAC-MAN',
    'file_type': 4,                      # DOS 3.3 binary
    'load_address': 0x1000,
    'length': 0x6A00,
    'body_sha256': 'EFF5B2635EDF0E3B6A227BEE0A477EB697AAEBC29EBC57089167DF5F10EE182C',
}

EXACT, PROGRAM, FAIL, UNREADABLE = 'exact-image', 'game-program', 'not-verified', 'unreadable'


def looks_like_dos33(img):
    v = dsk.sector(img, 17, 0)
    return v[1] == 17 and v[3] in (2, 3) and v[0x35] == 16 and v[0x34] == 35


def verify(img, expected=EXPECTED):
    """Returns (status, reason). Never raises on malformed input."""
    if len(img) != IMAGE_SIZE:
        return UNREADABLE, 'size is %d bytes, expected %d (only 140K DOS-order .dsk/.do images are supported)' % (len(img), IMAGE_SIZE)
    if hashlib.sha256(img).hexdigest().upper() == expected['image_sha256']:
        return EXACT, 'whole-image SHA-256 matches'
    if not looks_like_dos33(img):
        return FAIL, 'not a DOS 3.3 140K image in DOS sector order'
    try:
        entry = [e for e in dsk.catalog(img) if e[0] == expected['file_name']]
        if not entry:
            return FAIL, 'no file named %s in the catalog' % expected['file_name']
        if entry[0][1] != expected['file_type']:
            return FAIL, 'file %s is not a binary file' % expected['file_name']
        addr, n, body, _ = dsk.binary_body(img, expected['file_name'])
    except (KeyError, IndexError):
        return FAIL, 'catalog or file allocation is damaged'
    if addr != expected['load_address'] or n != expected['length'] or len(body) != n:
        return FAIL, 'header says load $%04X length $%04X, expected $%04X / $%04X' % (addr, n, expected['load_address'], expected['length'])
    if hashlib.sha256(body).hexdigest().upper() != expected['body_sha256']:
        return FAIL, 'game program bytes differ from the expected program'
    return PROGRAM, 'game program SHA-256 matches; the rest of the disk (DOS tracks, volume number, free space) differs from the reference image'


def main():
    try:
        img = open(sys.argv[1], 'rb').read()
    except OSError as e:
        print(UNREADABLE + ':', e)
        return 2
    status, reason = verify(img)
    print('%s: %s' % (status, reason))
    return 0 if status in (EXACT, PROGRAM) else 2 if status == UNREADABLE else 1


if __name__ == '__main__':
    sys.exit(main())
