"""TASK-021: run the analysis scenarios of the last days with coverage on and merge the result with the earlier coverage files
(development tool; uses original data from scratch/). Scenarios: all 48 ghost routines over their complete class tables, every sound list,
fruit set-up/pick-up/score for levels 1-30 and a forced fruit pick-up in play, energizer injection with ghost eating and the 10,000-point
bonus life, random play with deaths, a forced level completion. Writes scratch/cov_close.json (same format as the other cov files).
Usage: python -I tools/cov_close.py        (then: python -I tools/cov_union.py scratch/cov*.json)"""
import json, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
import play
from apple2 import Machine, NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from trace_coverage import runs

MACHINES = []


def factory(cov=False):
    mc = play.make_machine(cov=True)
    MACHINES.append(mc)
    return mc


def patch(*modules):
    for m in modules:
        m.make_machine = factory


def level_complete_scenario(seed=2):
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    state = {'armed': False}
    step = mc.cpu.step

    def hooked():
        if mc.cpu.pc == 0x7B15 and mc.m[0xE8] == 0 and not state['armed']:
            mc.m[0x56] = 3                       # three items from the end of the level
            state['armed'] = True
        return step()
    mc.cpu.step = hooked
    rng, done = random.Random(seed), 0
    start_level = mc.m[0x55]
    while done < 150_000_000 and mc.m[0xE8] == 0 and mc.m[0x55] == start_level:
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
        done += 150_000
    for _ in range(100):                          # keep playing a little in the new level
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    return mc.m[0x55] - start_level


def main():
    import ghost_rules, sound_model, trace_fruit, trace_eatscore, trace_pacman, trace_sound, trace_speed
    patch(sound_model, trace_fruit, trace_eatscore, trace_pacman, trace_sound, trace_speed)
    gm = Machine()
    gm.load(0x4000, open('scratch/extract/PAC-MAN.1000.bin', 'rb').read())
    gm.enable_coverage()
    MACHINES.append(gm)
    ghost_rules.build_tables(nsamples=20, mc=gm)
    print('ghost routines done', flush=True)
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mcs = factory()
    for name, how, eng, addr in sound_model.SOUNDS:
        sound_model.oracle_clicks(mcs, eng, addr)
    print('sounds done', flush=True)
    trace_fruit.per_level()
    trace_fruit.play(1, 60_000_000, 3)
    print('fruit done', flush=True)
    trace_eatscore.run(7, 100_000_000, want=1)
    print('eat/bonus done', flush=True)
    trace_pacman.run(1, 1, 40_000_000, quiet=True)
    trace_sound.run(1, 60_000_000)
    print('play/deaths done', flush=True)
    print('level advance:', level_complete_scenario())
    ex, rd, wr, ops = bytearray(65536), bytearray(65536), bytearray(65536), bytearray(65536)
    for mc in MACHINES:
        for dst, key in ((ex, 'exec'), (rd, 'read'), (wr, 'write'), (ops, 'opstart')):
            src = mc.cov[key]
            for a in range(65536):
                if src[a]:
                    dst[a] = 1
    json.dump({'exec': runs(ex), 'read': runs(rd), 'write': runs(wr), 'opstart': [a for a in range(65536) if ops[a]], 'cycles': 0},
              open('scratch/cov_close.json', 'w'))
    print('executed game-area bytes:', sum(ex[0x4000:0xAA00]))


if __name__ == '__main__':
    main()
