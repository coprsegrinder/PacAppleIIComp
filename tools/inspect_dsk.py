"""Read-only inspector for a 140K Apple II DOS 3.3-order disk image.
Usage: python -I tools/inspect_dsk.py <image.dsk>
Prints hashes, VTOC info, catalog, and binary-file headers. Never writes."""
import sys, hashlib

def main(path):
    d = open(path, "rb").read()
    print("file:", path, "size:", len(d))
    for a in ("sha256", "sha1", "md5"):
        print(a, hashlib.new(a, d).hexdigest().upper())
    sec = lambda t, s: d[(t*16+s)*256:(t*16+s+1)*256]
    v = sec(17, 0)
    print("VTOC: DOS release", v[3], "volume", v[6], "tracks", v[0x34], "sectors/track", v[0x35])
    t, s, seen = v[1], v[2], set()
    while t and (t, s) not in seen:
        seen.add((t, s)); c = sec(t, s)
        for i in range(7):
            e = c[11+i*35:11+(i+1)*35]
            if e[0] == 0: continue
            name = ''.join(chr(b & 0x7f) for b in e[3:33]).rstrip()
            data, tl, ts_ = b'', e[0], e[1]
            tsseen = set()
            while tl and (tl, ts_) not in tsseen:
                tsseen.add((tl, ts_)); l = sec(tl, ts_)
                for k in range(12, 256, 2):
                    if l[k] or l[k+1]: data += sec(l[k], l[k+1])
                tl, ts_ = l[1], l[2]
            print(f"file {name!r} type=${e[2]&0x7f:02X} locked={bool(e[2]&0x80)} sectors={e[33]|e[34]<<8} bytes_read={len(data)}")
            if (e[2] & 0x7f) == 4:
                a, n = data[0] | data[1] << 8, data[2] | data[3] << 8
                print(f"  binary: load ${a:04X} length ${n:04X} end ${a+n-1:04X}")
        t, s = c[1], c[2]

main(sys.argv[1])
