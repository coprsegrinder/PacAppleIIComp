"""TASK-021c: the last two reachable-looking unexecuted runs (development tool; original data from scratch/).
  C: Space pressed in the delay loop of the routine at $52BB (called from $43D6 in the attract sequence); the routine is entered directly.
  D: the hidden rank prompt answered with a single digit ($41D2); E: answered with '0' (rank rejected, prompt repeats at $41D9).
Writes scratch/cov_close5_<scenario>.json. Stops itself after 8 minutes. Usage: python -I tools/cov_close5.py C|D"""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import do_step
from trace_coverage import runs
from cov_close import factory, MACHINES
import explore

START = time.monotonic()


def save(name):
    ex, rd, wr, ops = bytearray(65536), bytearray(65536), bytearray(65536), bytearray(65536)
    for m_ in MACHINES:
        for dst, key in ((ex, 'exec'), (rd, 'read'), (wr, 'write'), (ops, 'opstart')):
            src = m_.cov[key]
            for a in range(65536):
                if src[a]:
                    dst[a] = 1
    json.dump({'exec': runs(ex), 'read': runs(rd), 'write': runs(wr), 'opstart': [a for a in range(65536) if ops[a]], 'cycles': 0},
              open('scratch/cov_close5_%s.json' % name, 'w'))


def scenario_c():
    mc = factory()
    for st in ('key:1', 'key:cr', '20_000_000'):
        do_step(mc, st, lambda *_: None)
    hits = {}
    step = mc.cpu.step
    state = {'sent': False}

    def hook():
        pc = mc.cpu.pc
        if pc == 0x52EB and not state['sent']:
            state['sent'] = True
            mc.push_key(0x20)                      # Space is waiting when the loop polls $C000
        if pc == 0x52F9:
            hits[pc] = hits.get(pc, 0) + 1
        return step()
    mc.cpu.step = hook
    c = mc.cpu
    c.push(0x6F); c.push(0x5D)                     # return to $6F5E (the key poll) if the routine ever returns; RTS adds 1
    c.pc = 0x52BB
    for _ in range(40):
        if hits or time.monotonic() - START > 8 * 60:
            break
        try:
            mc.run(100_000)
        except NeedKey:
            break
    print('C: $52F9 hits', hits.get(0x52F9, 0), '| key sent', state['sent'], flush=True)
    save('C')


def scenario_d(answer='7', name='D'):
    mc = factory()
    hits = {}
    step = mc.cpu.step

    def hook():
        if mc.cpu.pc == 0x41D2:
            hits[0x41D2] = hits.get(0x41D2, 0) + 1
        return step()
    mc.cpu.step = hook
    explore.to_players_prompt(mc)
    explore.hidden_menu(mc, 'R')
    explore.until_key(mc, 5_000_000)
    explore.send(mc, [answer, '\r'])
    explore.until_key(mc, 5_000_000)
    print('D: $41D2 hits', hits.get(0x41D2, 0), '| rank stored at $54E1 =', mc.m[0x54E1], flush=True)
    save(name)


if __name__ == '__main__':
    {'C': scenario_c, 'D': scenario_d, 'E': lambda: scenario_d('0', 'E')}[sys.argv[1]]()
    print('elapsed %.0f s' % (time.monotonic() - START))
