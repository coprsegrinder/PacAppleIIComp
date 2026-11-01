"""TASK-021, second pass: targeted scenarios for the last small unexecuted runs (development tool; original data from scratch/).
Scenarios: Space at the 'press RETURN' prompt, Ctrl-S, joystick buttons in both modes, a 2-player game with deaths, a death while a ghost is
eyes, a forced mode-timer expiry, the level clamps (levels 22+/26+) and the cartoon scheduler for high levels. Writes scratch/cov_close2.json.
Usage: python -I tools/cov_close2.py"""
import json, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import KEYSET
from play import do_step
from trace_coverage import runs
import cov_close
from cov_close import factory, MACHINES

SILENT = lambda *_: None


def new_game(players=1, mode=1, space_first=False):
    mc = factory()
    for st in ('key:%d' % mode, 'key:cr', '70_000_000'):
        do_step(mc, st, SILENT)
    seq = ['key:sp', '3_000_000', 'key:%d' % players, '3_000_000']
    if space_first:
        seq += ['key:sp', '5_000_000', 'key:%d' % players, '3_000_000']      # Space at the prompt goes back to the demo; start again
    seq += ['key:cr', '1_000_000']
    for st in seq:
        do_step(mc, st, SILENT)
    return mc


def play_blocks(mc, blocks, rng, buttons=False, extra=None, stop=None):
    for i in range(blocks):
        if stop and stop(mc):
            break
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        if buttons:
            mc.buttons[0] = rng.random() < .5
            mc.buttons[1] = rng.random() < .5
            mc.buttons[2] = rng.random() < .5
        if extra:
            extra(mc, i)
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)


def call(mc, addr, **regs):
    m, cpu = mc.m, mc.cpu
    m[0x0300:0x0304] = bytes([0x20, addr & 255, addr >> 8, 0xEA])
    cpu.pc, cpu.sp = 0x0300, 0xFD
    for k, v in regs.items():
        setattr(cpu, k, v)
    mc.run(30_000_000, stop_pc=0x0303)


def main():
    rng = random.Random(11)
    # 1. Space at the prompt, Ctrl-S twice (toggles the speaker operands there and back), buttons in keyboard mode
    mc = new_game(1, 1, space_first=True)
    play_blocks(mc, 40, rng, buttons=True)
    mc.push_key(0x93)
    play_blocks(mc, 5, rng)
    mc.push_key(0x93)
    play_blocks(mc, 5, rng)
    print('1 done', flush=True)
    # 2. joystick mode with all three buttons
    mc = new_game(1, 2)
    play_blocks(mc, 60, rng, buttons=True)
    print('2 done', flush=True)
    # 3. two players: random play until at least two deaths (the player switch code)
    mc = new_game(2, 1)
    play_blocks(mc, 700, rng, stop=lambda m: m.m[0xE8] == 1)
    print('3 done, lives', mc.m[0x58], flush=True)
    # 4. death while ghost 1 is eyes, forced mode-timer expiry in both phases
    mc = new_game(1, 1)
    hit = {'eyes': False}

    def extra(mc, i):
        if i == 5:
            mc.m[0xE9], mc.m[0xEA], mc.m[0xEB] = 0, 1, 0            # chase phase ends next tick -> switch to corner phase
        if i == 20:
            mc.m[0xE9], mc.m[0xEA], mc.m[0xEB] = 1, 1, 0            # corner phase ends next tick
    step = mc.cpu.step

    def hooked():
        if mc.cpu.pc == 0x805D and mc.m[0xE8] == 0 and not hit['eyes']:
            hit['eyes'] = True
            for rec in (0x84, 0x98, 0xAC, 0xC0):
                mc.m[rec + 9] = 1                                     # every ghost is eyes when the death sequence starts
        return step()
    mc.cpu.step = hooked
    play_blocks(mc, 500, rng, extra=extra, stop=lambda m: m.m[0xE8] == 1)
    print('4 done, eyes at death:', hit['eyes'], flush=True)
    # 5. ghost house exit in the chase phase: start a game with the corner phase already over
    mc = new_game(1, 1)
    mc.m[0xE9], mc.m[0xEA], mc.m[0xEB] = 0, 255, 255
    play_blocks(mc, 150, rng, stop=lambda m: m.m[0xE8] == 1)
    print('5 done', flush=True)
    # 6. level clamps and the cartoon scheduler for high levels, standalone
    mc = factory()
    for level in (22, 26, 30, 40):
        mc.m[0x55] = level
        call(mc, 0x6D1E)
        call(mc, 0x6D27)
    mc.m[0x55] = 30
    call(mc, 0x6F13)
    print('6a done', flush=True)
    for level in (10, 13, 14, 17, 18, 22, 30):
        mc.m[0x55] = level
        try:
            call(mc, 0x8155)
        except Exception as e:
            print('   cartoon scheduler at level', level, 'stopped:', type(e).__name__, flush=True)
    print('6 done', flush=True)
    ex, rd, wr, ops = bytearray(65536), bytearray(65536), bytearray(65536), bytearray(65536)
    for m_ in MACHINES:
        for dst, key in ((ex, 'exec'), (rd, 'read'), (wr, 'write'), (ops, 'opstart')):
            src = m_.cov[key]
            for a in range(65536):
                if src[a]:
                    dst[a] = 1
    json.dump({'exec': runs(ex), 'read': runs(rd), 'write': runs(wr), 'opstart': [a for a in range(65536) if ops[a]], 'cycles': 0},
              open('scratch/cov_close2.json', 'w'))
    print('executed game-area bytes:', sum(ex[0x4000:0xAA00]))


if __name__ == '__main__':
    main()
