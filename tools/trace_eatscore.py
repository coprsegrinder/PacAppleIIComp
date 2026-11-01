"""Verify ghost-eating scores and the bonus life on the oracle (development tool; uses original data from scratch/).
Injects energizers into the maze while Pac-Man plays under random keys, logs the score before/after each ghost eaten ($8B9D..$8BD9),
and presets the score to 9,980 to see the 10,000-point bonus life. Stops as soon as the demo ($E8 <> 0) takes over.
Usage: python -I tools/trace_eatscore.py [seed] [max_instr]
"""
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from autoplay import start_game, KEYSET


def run(seed=7, n=100_000_000, want=4, preset_score=True):
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    log, st = [], {'before': None, 'bonus': None, 'preset': False}
    step = mc.cpu.step
    sc = lambda: mc.m[0x54] << 16 | mc.m[0x53] << 8 | mc.m[0x52]

    def hooked():
        pc = mc.cpu.pc
        if pc == 0x7ABD and mc.m[0xE8] == 0:
            if preset_score and not st['preset']:
                mc.m[0x54], mc.m[0x53], mc.m[0x52] = 0, 0x99, 0x80
                st['preset'] = True
                st['bonus'] = ('preset', mc.m[0x58])
            elif mc.m[0x78] == 0 and mc.m[0x56] % 8 == 0:
                mc.m[mc.m[0x16] | mc.m[0x17] << 8] = 131
        if pc == 0x8BBD and mc.m[0xE8] == 0:
            st['before'] = (sc(), mc.m[0x68])
        if pc == 0x8BDC and st['before'] and mc.m[0xE8] == 0:
            log.append({'score_before': st['before'][0], 'step': st['before'][1], 'score_after': sc(), 'step_after': mc.m[0x68]})
            st['before'] = None
        return step()
    mc.cpu.step = hooked
    rng, done, lives_seen = random.Random(seed), 0, set()
    while done < n and mc.m[0xE8] == 0 and len(log) < want:
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
        lives_seen.add((mc.m[0x58], sc()))
        done += 150_000
    return log, st['bonus'], sorted(lives_seen), mc.m[0xE8]


if __name__ == '__main__':
    log, bonus, lives, e8 = run(int(sys.argv[1]) if len(sys.argv) > 1 else 7, int(sys.argv[2].replace('_', '')) if len(sys.argv) > 2 else 100_000_000)
    for l in log:
        print('ghost eaten: score %06X -> %06X (step $68 %d -> %d)' % (l['score_before'], l['score_after'], l['step'], l['step_after']))
    print('bonus preset:', bonus, '| (lives $58, score) samples:', lives[:3], '...', lives[-3:], '| E8 at end', e8)
