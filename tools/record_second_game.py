"""TASK-029g15: does the high score survive from one game to the next? (development tool; original data from scratch/)
Plays game 1 (A's keys, lives poked to 1 at tick 5 so that it ends), lets it end (attract mode), starts game 2 the usual way and saves
scratch/page_tick0_game2.bin (the hi-res page at the first tick of game 2) and scratch/high_game2.txt ($00-$02 at the end of game 1 and at the first tick of game 2,
the score of game 1, and the score at the first tick of game 2).  Usage: python -I tools/record_second_game.py   (about 3-4 minutes)"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game
from play import do_step
from cov_close import factory

mc = factory()
for st in ('key:1', 'key:cr', '70_000_000'):
    do_step(mc, st, lambda *_: None)
start_game(mc)
inputs = json.load(open('tests/golden/gameplay_traces.json'))['scenarios']['A']['inputs']
by_tick = {}
for t, k in inputs:
    by_tick.setdefault(t, []).append(k)
st = {'t': 0, 'phase': 1, 'ended': None, 'g2': None}
step = mc.cpu.step
info = {}


def hook():
    if mc.cpu.pc == 0x7861:
        if st['phase'] == 1 and mc.m[0xE8] == 0:
            t = st['t']
            if t == 5:
                mc.m[0x58] = 0x81
            for k in by_tick.get(t, []):
                mc.push_key(k)
            st['t'] = t + 1
        elif st['phase'] == 1 and mc.m[0xE8] != 0 and st['t'] > 10:
            st['phase'] = 2
            info['end_of_game1'] = {'ticks': st['t'], 'hi': list(mc.m[0:3]), 'score': list(mc.m[0x52:0x55])}
        elif st['phase'] == 3 and mc.m[0xE8] == 0:
            info['game2_tick0'] = {'hi': list(mc.m[0:3]), 'score': list(mc.m[0x52:0x55]), 'lives': mc.m[0x58], 'level': mc.m[0x55]}
            open('scratch/page_tick0_game2.bin', 'wb').write(bytes(mc.m[0x2000:0x4000]))
            raise SystemExit
    return step()


mc.cpu.step = hook
try:
    for _ in range(4000):
        if st['phase'] == 2:
            for _ in range(40):                  # a few seconds of attract mode, then start game 2 as from the title screen
                try:
                    mc.run(150_000)
                except NeedKey:
                    mc.push_key(13)
            st['phase'] = 3
            for s in ('key:sp', '3_000_000', 'key:1', '3_000_000', 'key:cr', '1_000_000'):
                do_step(mc, s, lambda *_: None)
            continue
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
except SystemExit:
    pass
open('scratch/high_game2.txt', 'w').write(json.dumps(info))
print(info)
