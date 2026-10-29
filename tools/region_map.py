"""Heuristic page-by-page map of a 6502 binary: guesses code vs. data vs. empty.
Usage: python -I tools/region_map.py <body.bin> <origin-hex>
Heuristics only - every label is a hint to be confirmed by disassembly or emulator tracing."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import disassemble, OPS

def page_stats(page, origin):
    zero = page.count(0) / len(page)
    ff = page.count(0xFF) / len(page)
    text = sum(1 for b in page if 0xA0 <= b <= 0xFA or 0x20 <= b <= 0x7A) / len(page)  # ASCII, either bit-7 polarity
    # linear sweep from page start: fraction of bytes inside valid documented opcodes, and
    # fraction of JSR/JMP absolute operands pointing at plausible places (program range or ROM/I-O)
    ins = disassemble(page, origin)
    valid = sum(len(r) for _, r, t in ins if not t.startswith('.byte')) / len(page)
    jumps = [t for _, _, t in ins if t.startswith(('JSR $', 'JMP $'))]
    good = sum(1 for t in jumps if (lambda a: 0x4000 <= a <= 0xAAFF or a >= 0xC000 or a < 0x0100)(int(t.split('$')[1], 16)))
    return zero, ff, text, valid, len(jumps), good

def classify(zero, ff, text, valid, nj, good):
    if zero > 0.98: return 'empty'
    if zero > 0.6: return 'sparse data'
    if nj >= 2 and good / nj > 0.8 and valid > 0.9 and zero < 0.3: return 'code?'
    if text > 0.9 and zero < 0.1: return 'text?'
    return 'data?' if valid < 0.9 or zero > 0.3 else 'mixed'

if __name__ == '__main__':
    body = open(sys.argv[1], 'rb').read(); origin = int(sys.argv[2], 16)
    print("| Addr | zero% | valid-op% | JSR/JMP (plausible/total) | guess |\n|---|---|---|---|---|")
    for i in range(0, len(body), 256):
        p = body[i:i + 256]; st = page_stats(p, origin + i)
        print(f"| ${origin+i:04X} | {st[0]*100:.0f} | {st[3]*100:.0f} | {st[5]}/{st[4]} | {classify(*st)} |")
