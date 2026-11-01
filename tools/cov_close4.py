"""TASK-021b: force the last reachable-looking unexecuted runs (development tool; original data from scratch/). Each scenario reports whether
its target instruction ran. Scenarios: (A) a ghost is eaten while the others are eyes ($806F), (B) a ghost-house exit of ghost 2/4 while
it moves up ($89AC), (C) Space pressed during the wait loop at $52E9 ($52F9), (D) the hidden rank prompt answered with ONE digit ($41D2).
Writes scratch/cov_close4.json. Stops itself after 15 minutes. Usage: python -I tools/cov_close4.py"""
import json, os, random, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from trace_coverage import runs
from cov_close import factory, MACHINES
import explore

START = time.monotonic()
SILENT = lambda *_: None


def over_budget():
    return time.monotonic() - START > 15 * 60


def fresh(mode='1'):
    mc = factory()
    for st in ('key:%s' % mode, 'key:cr', '70_000_000'):
        do_step(mc, st, SILENT)
    return mc


def play_until(mc, hits, target, rng, limit_blocks=400, inject=False):
    for _ in range(limit_blocks):
        if hits.get(target, 0) >= 1 or mc.m[0xE8] == 1 or over_budget():
            break
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)


def main():
    rng = random.Random(1)
    hits = {}
    # A. eaten-ghost freeze with other ghosts already eyes
    mc = fresh()
    start_game(mc)
    step = mc.cpu.step

    def hookA():
        pc, m = mc.cpu.pc, mc.m
        if m[0xE8] == 0:
            if pc == 0x7ABD and m[0x78] == 0 and m[0x56] % 12 == 0:
                m[m[0x16] | m[0x17] << 8] = 131                         # inject an energizer now and then
            if pc == 0x805D:
                for r in (0x84, 0x98, 0xAC, 0xC0):
                    m[r + 9] = 1                                          # the other ghosts are eyes (an earlier ghost was eaten)
            if pc == 0x806F:
                hits[0x806F] = hits.get(0x806F, 0) + 1
        return step()
    mc.cpu.step = hookA
    play_until(mc, hits, 0x806F, rng)
    print('A: $806F hits', hits.get(0x806F, 0), flush=True)
    # B. ghost 2 or 4 leaves the house while its direction is 'up'
    mc = fresh()
    start_game(mc)
    step = mc.cpu.step

    def hookB():
        pc, m = mc.cpu.pc, mc.m
        if pc == 0x8988 and m[0xE8] == 0 and m[0xE0] in (2, 4) and m[0x72] == 1 and m[0x81] == 1:
            m[0x82] = 1                                                   # its house countdown ends on this step
        if pc == 0x89AC:
            hits[0x89AC] = hits.get(0x89AC, 0) + 1
        return step()
    mc.cpu.step = hookB
    play_until(mc, hits, 0x89AC, rng, 150)
    print('B: $89AC hits', hits.get(0x89AC, 0), flush=True)
    # C. Space during the delay loop at $52E9
    mc = fresh()
    state = {'sent': False}
    step = mc.cpu.step

    def hookC():
        pc = mc.cpu.pc
        if pc == 0x52EE and not state['sent']:
            state['sent'] = True
            mc.push_key(0x20)
        if pc == 0x52F9:
            hits[0x52F9] = hits.get(0x52F9, 0) + 1
        return step()
    mc.cpu.step = hookC
    for st in ('key:sp', '3_000_000', 'key:1', '3_000_000', 'key:cr', '3_000_000'):
        do_step(mc, st, SILENT)
    for _ in range(100):
        if hits.get(0x52F9) or over_budget():
            break
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    print('C: $52EE reached', state['sent'], '| $52F9 hits', hits.get(0x52F9, 0), flush=True)
    # D. hidden rank prompt with a single digit
    mc = factory()
    step = mc.cpu.step

    def hookD():
        if mc.cpu.pc == 0x41D2:
            hits[0x41D2] = hits.get(0x41D2, 0) + 1
        return step()
    mc.cpu.step = hookD
    explore.to_players_prompt(mc)
    explore.hidden_menu(mc, 'R')
    explore.until_key(mc, 5_000_000)
    explore.send(mc, ['7', '\r'])
    explore.until_key(mc, 5_000_000)
    print('D: $41D2 hits', hits.get(0x41D2, 0), '| rank stored at $54E1 =', mc.m[0x54E1], flush=True)
    ex, rd, wr, ops = bytearray(65536), bytearray(65536), bytearray(65536), bytearray(65536)
    for m_ in MACHINES:
        for dst, key in ((ex, 'exec'), (rd, 'read'), (wr, 'write'), (ops, 'opstart')):
            src = m_.cov[key]
            for a in range(65536):
                if src[a]:
                    dst[a] = 1
    json.dump({'exec': runs(ex), 'read': runs(rd), 'write': runs(wr), 'opstart': [a for a in range(65536) if ops[a]], 'cycles': 0},
              open('scratch/cov_close4.json', 'w'))
    print('elapsed %.0f s' % (time.monotonic() - START))


if __name__ == '__main__':
    main()
