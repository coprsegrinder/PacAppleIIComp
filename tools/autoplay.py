"""Deterministic random auto-player for coverage (development tool; uses original data from scratch/).
Usage: python -I tools/autoplay.py <mode 1|2> <games> <seed> <out.json> [max_instr_per_game]
mode 1 = keyboard (random A / Z / left / right keys), mode 2 = Apple joystick (random paddle values and button presses).
Plays `games` games from the attract loop (space, players=1, Enter), recording coverage; writes exec/read/write run lists to <out.json>.
"""
import json, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from trace_coverage import runs

KEYSET = ['A', 'Z', 0x08, 0x15]


def start_game(mc):
    for st in ('key:sp', '3_000_000', 'key:1', '3_000_000', 'key:cr', '1_000_000'):
        do_step(mc, st, lambda *_: None)


def play_one(mc, mode, rng, max_instr, step=150_000):
    done = 0
    while done < max_instr:
        if mc.m[0xE8] == 1:                     # back in demo/attract: game over
            return done, True
        if mode == 1:
            if rng.random() < 0.5:
                mc.push_key(rng.choice(KEYSET))
        else:
            mc.paddles[0] = rng.choice([0, 127, 255])
            mc.paddles[1] = rng.choice([0, 127, 255])
            mc.buttons[0] = rng.random() < 0.2
        try:
            mc.run(step)
        except NeedKey:
            mc.push_key(0x0D)
        done += step
    return done, False


def main():
    mode, games, seed, out = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    max_instr = int(sys.argv[5]) if len(sys.argv) > 5 else 60_000_000
    rng = random.Random(seed)
    mc = make_machine(cov=True)
    for st in ('key:%d' % mode, 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    for g in range(games):
        start_game(mc)
        n, over = play_one(mc, mode, rng, max_instr)
        print('game %d: %.1fM instr, game over reached: %s, speaker clicks %d' % (g + 1, n / 1e6, over, len(mc.speaker)), flush=True)
        if not over:
            break
        mc.run(2_000_000)
    c = mc.cov
    json.dump({'exec': runs(c['exec']), 'read': runs(c['read']), 'write': runs(c['write']),
               'opstart': [a for a in range(65536) if c['opstart'][a]], 'cycles': mc.cpu.cycles, 'mode': mode, 'seed': seed},
              open(out, 'w'))
    print('executed game-area bytes:', sum(c['exec'][0x4000:0xAA00]), 'unknown io:', mc.unknown_io, 'rom calls:', mc.rom_calls)


if __name__ == '__main__':
    main()
