"""TASK-027: expected values for the C++ hires tests, computed with the verified Python models (development tool; original data from scratch/).
Prints SHA-256 hashes of derived pages (not original data) for: the wall page in both modes, and the page after printing the first N attract-screen
string records (starting at $434F) onto a black ($80) page. Usage: python -I tools/make_hires_vectors.py"""
import hashlib, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import wall_model, text_model

body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()


def table(addr):
    return body[addr - 0x4000: addr - 0x4000 + 426]


def records(start, n):
    out, a = [], start
    for _ in range(n):
        out.append(a)
        i = a - 0x4000 + 2
        while body[i] < 0x80:
            i += 1
        a = 0x4000 + i + 1
    return out, a


if __name__ == '__main__':
    print('wall normal  ', hashlib.sha256(wall_model.wall_page(table(0x5F69), 1)).hexdigest().upper())
    print('wall forced  ', hashlib.sha256(wall_model.wall_page(table(0x5DBF), 0)).hexdigest().upper())
    for n in (1, 5, 12):
        recs, end = records(0x434F, n)
        page = bytearray([0x80]) * 0x2000
        for r in recs:
            text_model.draw_string(page, body, r)
        print('text first %2d records (end $%04X) ' % (n, end), hashlib.sha256(bytes(page)).hexdigest().upper())
