"""Extract a binary file body from a disk image into an output directory (outside git).
Usage: python -I tools/extract_binary.py <image.dsk> <FILENAME> <outdir>"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import dsk

img_path, name, outdir = sys.argv[1:4]
img = open(img_path, 'rb').read()
addr, n, body, tail = dsk.binary_body(img, name)
os.makedirs(outdir, exist_ok=True)
out = os.path.join(outdir, name.replace(' ', '_') + f'.{addr:04X}.bin')
open(out, 'wb').write(body)
print(f"{name}: load ${addr:04X} length ${n:04X}; wrote {len(body)} bytes to {out}")
print("body sha256:", dsk.sha256(body))
print(f"trailing bytes after declared length: {len(tail)} (all zero: {not any(tail)})")
