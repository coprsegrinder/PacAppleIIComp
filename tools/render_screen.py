"""Render the hi-res page 1 screen ($2000-$3FFF) of the headless machine after N instructions to a PNG (scratch/, git-ignored).
Same keys as the AppleWin comparison: '1' then Enter. Usage: python -I tools/render_screen.py <instructions> <out.png> [scale]
Development tool; uses original data from scratch/.
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey
from render_frames import pixels, write_png


def hires_base(y):
    return 0x2000 + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


def screen_rows(m, page=0x2000, scale=2):
    out = []
    for y in range(192):
        b = hires_base(y) - 0x2000 + page
        row = pixels(bytes(m[b:b + 40]), 40, 1, 0)[0]
        row = [px for px in row for _ in range(scale)]
        out.extend([row] * scale)
    return out


def main():
    n = int(sys.argv[1])
    path = sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    mc.run(10_000_000, stop_pc=0x4027)
    mc.push_key('1')
    mc.push_key(0x0D)
    try:
        mc.run(10_000_000)
    except NeedKey:
        pass
    mc.run(n)
    write_png(path, screen_rows(mc.m, scale=scale))
    print('cycles', mc.cpu.cycles, 'switches', mc.switches)


if __name__ == '__main__':
    main()
