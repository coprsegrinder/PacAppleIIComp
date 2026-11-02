"""Make the DOS 3.3 binary file (.b33: load address, length, body) that apple2tc/a2run load, from the verified disk image.
Usage: python -I tools/make_b33.py <image.dsk> <FILENAME> <out.b33>   (output stays in git-ignored scratch/: it contains original code)"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import dsk

img = open(sys.argv[1], 'rb').read()
addr, n, body, tail = dsk.binary_body(img, sys.argv[2])
open(sys.argv[3], 'wb').write(struct.pack('<HH', addr, n) + body)
print(f"{sys.argv[2]}: load ${addr:04X} length ${n:04X}; wrote {4 + len(body)} bytes to {sys.argv[3]}; body sha256 {dsk.sha256(body)}")
