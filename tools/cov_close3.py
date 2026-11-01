"""TASK-021, third pass (development tool; original data from scratch/): the digital-joystick read ($6FD0), Space at the prompt,
a forced mode-timer expiry and a death with eyes ghosts, each with diagnostics. Writes scratch/cov_close3.json.
Usage: python -I tools/cov_close3.py"""
import json, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import do_step
from trace_coverage import runs
from cov_close import factory, MACHINES
from cov_close2 import SILENT, play_blocks

READS = {0x6FDF: 2, 0x6FE9: 3, 0x6FF3: 1, 0x6FFD: 0}       # instruction -> bit of the 4-bit mask that read supplies (bits 2,3,1,0 in the code)


def start(mc, players=1):
    for st in ('key:sp', '3_000_000', 'key:%d' % players, '3_000_000', 'key:cr', '1_000_000'):
        do_step(mc, st, SILENT)


def main():
    rng = random.Random(5)
    # a. space at the 'PRESS <RETURN> TO PLAY OR <SPACE> TO GO BACK TO DEMO' prompt, then a normal start
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000', 'key:sp', '3_000_000', 'key:1', '3_000_000', 'key:sp', '5_000_000'):
        do_step(mc, st, SILENT)
    print('a: E8 after space at the prompt =', mc.m[0xE8], flush=True)
    # b. digital joystick: feed every 4-bit combination to the four $C061 reads of $6FD0
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, SILENT)
    start(mc)
    state = {'mask': 1, 'decoded': {}}
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        if pc in READS:
            mc.buttons[0] = bool((state['mask'] >> READS[pc]) & 1)
        if pc == 0x7004:                                              # TAX with A = the assembled mask
            state['mask_seen'] = mc.cpu.a
        if pc in (0x700A, 0x700C) and 'mask_seen' in state:
            state['decoded'][state['mask_seen']] = 'none' if pc == 0x700A else 'direction'
            state['mask'] = (state['mask'] + 1) % 16
        return step()
    mc.cpu.step = hooked
    play_blocks(mc, 150, rng)
    print('b: masks seen', sorted(state['decoded'].items()), flush=True)
    # c. forced mode-timer expiry with diagnostics
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, SILENT)
    start(mc)
    hits = {}
    step = mc.cpu.step

    def hooked2():
        pc = mc.cpu.pc
        if pc in (0x808C, 0x80A1, 0x80B1, 0x805D, 0x806F):
            hits[pc] = hits.get(pc, 0) + 1
        return step()
    mc.cpu.step = hooked2

    def extra(mc, i):
        if i in (3, 10, 17):
            mc.m[0xE9], mc.m[0xEA], mc.m[0xEB] = (0, 1, 0) if i != 10 else (1, 1, 0)
    play_blocks(mc, 200, rng, extra=extra, stop=lambda m: m.m[0xE8] == 1)
    print('c: hits', {hex(k): v for k, v in hits.items()}, 'E8', mc.m[0xE8], flush=True)
    # d. death while ghosts are eyes: force the eyes flag of every ghost as soon as Pac-Man is caught ($8BDD)
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, SILENT)
    start(mc)
    seen = {'caught': 0, '805D': 0}
    step = mc.cpu.step

    def hooked3():
        pc = mc.cpu.pc
        if pc == 0x8BDD and mc.m[0xE8] == 0:
            seen['caught'] += 1
            for rec in (0x84, 0x98, 0xAC, 0xC0):
                mc.m[rec + 9] = 1
            mc.m[0x79] = 1
        if pc == 0x805D:
            seen['805D'] += 1
        return step()
    mc.cpu.step = hooked3
    play_blocks(mc, 600, rng, stop=lambda m: m.m[0xE8] == 1)
    print('d: caught', seen, flush=True)
    ex, rd, wr, ops = bytearray(65536), bytearray(65536), bytearray(65536), bytearray(65536)
    for m_ in MACHINES:
        for dst, key in ((ex, 'exec'), (rd, 'read'), (wr, 'write'), (ops, 'opstart')):
            src = m_.cov[key]
            for a in range(65536):
                if src[a]:
                    dst[a] = 1
    json.dump({'exec': runs(ex), 'read': runs(rd), 'write': runs(wr), 'opstart': [a for a in range(65536) if ops[a]], 'cycles': 0},
              open('scratch/cov_close3.json', 'w'))
    print('executed game-area bytes:', sum(ex[0x4000:0xAA00]))


if __name__ == '__main__':
    main()
