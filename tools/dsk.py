"""Read-only helpers for 140K DOS 3.3-order disk images."""
import hashlib

def sector(img, t, s):
    return img[(t * 16 + s) * 256:(t * 16 + s + 1) * 256]

def catalog(img):
    """Yield (name, type, ts_list_track, ts_list_sector) for each active catalog entry."""
    v = sector(img, 17, 0)
    t, s, seen = v[1], v[2], set()
    while t and (t, s) not in seen:
        seen.add((t, s)); c = sector(img, t, s)
        for i in range(7):
            e = c[11 + i * 35:11 + (i + 1) * 35]
            if e[0] in (0, 0xFF): continue          # 0 = unused, 0xFF = deleted
            yield ''.join(chr(b & 0x7F) for b in e[3:33]).rstrip(), e[2] & 0x7F, e[0], e[1]
        t, s = c[1], c[2]

def data_sectors(img, t, s):
    """Return the list of (track, sector) holding a file's data, following its T/S list chain."""
    out, seen = [], set()
    while t and (t, s) not in seen:
        seen.add((t, s)); l = sector(img, t, s)
        out += [(l[k], l[k + 1]) for k in range(12, 256, 2) if l[k] or l[k + 1]]
        t, s = l[1], l[2]
    return out

def read_file(img, name):
    for n, ty, t, s in catalog(img):
        if n == name:
            return b''.join(sector(img, a, b) for a, b in data_sectors(img, t, s))
    raise KeyError(name)

def binary_body(img, name):
    """Return (load_address, declared_length, body_bytes, trailing_bytes) of a DOS 3.3 binary file."""
    f = read_file(img, name)
    addr, n = f[0] | f[1] << 8, f[2] | f[3] << 8
    return addr, n, f[4:4 + n], f[4 + n:]

def sha256(b):
    return hashlib.sha256(b).hexdigest().upper()
