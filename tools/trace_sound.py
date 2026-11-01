"""Check, in real play on the oracle, which sounds fire and that their click times equal the model (development tool).
Hooks the sound entry points; for each sound that starts, the clicks recorded until the next hook (or the model length) are compared with
tools/sound_model.py's closed-form model of the same list. Energizers are injected now and then so that the energizer sound occurs.
Usage: python -I tools/trace_sound.py [seed] [max_instr]
"""
import os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from autoplay import start_game, KEYSET
import sound_model as sm

ENTRIES = {0x7BDC: ('dot eaten', 'A', 0x7BE3), 0x7BE6: ('death', 'A', 0x7BED), 0x7C45: ('energizer', 'A', 0x7C4C),
           0x7C61: ('sound 7C61', 'A', 0x7C68), 0x7CF0: ('ghost/cartoon 7CF0', 'A', 0x7C9D)}


def run(seed=1, n=60_000_000):
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    events, table_calls, stray = [], [], []
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        if mc.m[0xE8] == 0:
            if pc == 0x7ABD and mc.m[0x56] % 12 == 0:
                mc.m[mc.m[0x16] | mc.m[0x17] << 8] = 131
            if pc in ENTRIES:
                events.append((pc, mc.cpu.cycles, len(mc.speaker)))
            if pc == 0x7C82:
                table_calls.append((mc.m[0x0A], mc.cpu.cycles, len(mc.speaker)))
        before = len(mc.speaker)
        c = step()
        if len(mc.speaker) != before and pc not in (0x7BB5, 0x7D06):
            stray.append(pc)                     # a click made by any instruction other than the two BIT $C030
        return c
    mc.cpu.step = hooked
    rng, done = random.Random(seed), 0
    while done < n and mc.m[0xE8] == 0:
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
        done += 150_000
    clicks = list(mc.speaker)
    results = {}
    for pc, cyc, idx in events:
        name, eng, addr = ENTRIES[pc]
        notes = sm.read_list(body, addr, eng)
        want = sm.model_clicks(eng, notes, None, addr)
        got = [c - clicks[idx] for c in clicks[idx:idx + len(want)]]
        results.setdefault(name, []).append(got == want)
    tabs = {}
    for idx_val, cyc, idx in table_calls:
        tabs.setdefault(idx_val, 0)
        tabs[idx_val] += 1
    return results, tabs, stray, len(clicks)


if __name__ == '__main__':
    res, tabs, stray, nclicks = run(int(sys.argv[1]) if len(sys.argv) > 1 else 1, int(sys.argv[2].replace('_', '')) if len(sys.argv) > 2 else 60_000_000)
    for k, v in res.items():
        print('%-20s fired %3d times, click times equal the model in %3d' % (k, len(v), sum(v)))
    print('table-sound calls ($0A value -> count):', tabs)
    print('clicks in total:', nclicks, '| clicks made by an instruction other than the two engine BITs:', len(stray))
