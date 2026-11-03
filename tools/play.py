"""Scripted headless session on the extracted game (development tool; uses original data from scratch/).
Usage: python -I tools/play.py [--cov] step [step ...]
  steps:  <N>           run N instructions (N may use _ e.g. 20_000_000)
          key:<c>       type a key ('sp' = space, 'cr' = Enter, otherwise the character)
          shot:<png>    render hi-res page 1 (or page 2 if selected) to scratch/<png>
          text          print the text page (page 2 if selected)
          info          print cycles, soft-switch state, speaker clicks, PC
  --cov turns on coverage and writes scratch/cov_play.json at the end (exec/read/write run lists).
Example: python -I tools/play.py --cov key:1 key:cr 70_000_000 shot:a.png key:sp 20_000_000 shot:b.png info
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey
from cpu6502 import CPUError
from render_frames import write_png
from render_screen import screen_rows
from trace_coverage import runs

KEYS = {'sp': 0x20, 'cr': 0x0D}


def make_machine(cov=False, real_rom=False):
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = Machine(real_rom)
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    mc.run(10_000_000, stop_pc=0x4027)
    if cov:
        mc.enable_coverage()
    return mc


def do_step(mc, st, out=print):
    if st.startswith('key:'):
        k = st[4:]
        mc.push_key(KEYS[k] if k in KEYS else k)
    elif st.startswith('shot:'):
        page = 0x4000 if mc.switches['page2'] else 0x2000
        write_png('scratch/' + st[5:], screen_rows(mc.m, page=page, scale=2))
    elif st == 'text':
        base = 0x800 if mc.switches['page2'] else 0x400
        for r in range(24):
            b = base + (r % 8) * 0x80 + (r // 8) * 0x28
            line = ''.join(chr(c & 0x7F) if 0x20 <= (c & 0x7F) < 0x7F else '.' for c in mc.m[b:b + 40]).rstrip()
            if line:
                out('%2d %s' % (r, line))
    elif st == 'info':
        out('cycles %d pc=%04X switches %s speaker clicks %d keys pending %d' % (
            mc.cpu.cycles, mc.cpu.pc, mc.switches, len(mc.speaker), len(mc.keys)))
    else:
        try:
            mc.run(int(st.replace('_', '')))
        except NeedKey:
            out('(waiting in RDKEY at pc=%04X)' % mc.cpu.pc)
        except CPUError as e:
            out('CPU error: %s' % e)


def main():
    args = sys.argv[1:]
    cov = '--cov' in args
    steps = [a for a in args if a != '--cov']
    mc = make_machine(cov)
    for st in steps:
        do_step(mc, st)
    if cov:
        c = mc.cov
        json.dump({'exec': runs(c['exec']), 'read': runs(c['read']), 'write': runs(c['write']),
                   'opstart': [a for a in range(65536) if c['opstart'][a]], 'cycles': mc.cpu.cycles},
                  open('scratch/cov_play.json', 'w'))
        print('coverage written; executed game-area bytes:', sum(c['exec'][0x4000:0xAA00]))


if __name__ == '__main__':
    main()
