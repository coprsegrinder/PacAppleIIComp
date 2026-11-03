"""TASK-033-4e: what the original does when Space is pressed in the attract loop (development tool; original data from scratch/).
Runs the attract loop, latches a key at T0 + <offset>, and records: the cycle of the key poll that takes it ($6F77 reads $C000 with bit 7 set), the cycles of $4144 / $4217 /
the players-prompt key wait ($4233) and the text page 2 ($0800) there, then types <digit> and records the PRESS-RETURN screen (key wait $4261), then types <last> (sp = back to the demo,
cr = start of the game) and records the cycle of $43D2 / $52BB / $52FC.   The monitor routines are stand-ins without cycles (tools/apple2.py), so the cycles are the game's own instructions only.
Usage: python -I tools/capture_prompts.py <name> <offset from T0> <digit 1-5> <sp|cr>      writes scratch/prompts_<name>.json
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

NAME, OFF, DIGIT, LAST = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
mc = make_machine(real_rom=os.environ.get('A2ROM') == '1')   # A2ROM=1: the real Autostart Monitor ROM runs instead of the stand-ins (TASK-033-4e3)
step = mc.cpu.step
st = {'t0': None, 'pressed': None, 'marks': []}


def hook():
    c = mc.cpu
    if st['t0'] is None:
        if c.pc == 0x52BB and mc.m[0xE8] == 1:
            st['t0'] = c.cycles
    else:
        if st['pressed'] is None and c.cycles - st['t0'] >= OFF:
            mc.push_key(0x20)
            mc._latch_key()
            st['pressed'] = c.cycles - st['t0']
        if c.pc in (0x6FAE, 0x4144, 0x4217, 0x43D2, 0x52BB, 0x52FC, 0x4233, 0x4261):
            st['marks'].append([c.pc, c.cycles - st['t0']])
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])


def page():
    return bytes(mc.m[0x800:0xC00]).hex()


out = {'offset': OFF, 'digit': int(DIGIT), 'pages': {}}
try:
    while True:
        mc.run(200_000)
except NeedKey:
    pass
out['pressed_at'] = st['pressed']
out['pc_wait'] = mc.cpu.pc
out['cycle_wait'] = mc.cpu.cycles - st['t0']
out['pages']['players'] = page()
out['zp1'] = bytes(mc.m[0:256]).hex()
out['switches'] = dict(mc.switches)
mc.push_key(DIGIT)
try:
    while True:
        mc.run(200_000)
except NeedKey:
    pass
out['pc_wait2'] = mc.cpu.pc
out['cycle_wait2'] = mc.cpu.cycles - st['t0']
out['pages']['press'] = page()
out['zp2'] = bytes(mc.m[0:256]).hex()
out['zp51'] = mc.m[0x51]
mc.push_key(0x20 if LAST == 'sp' else 0x0D)
stop = 0x52BB if LAST == 'sp' else 0x52FC
try:
    for _ in range(100):
        mc.run(10_000)
        if mc.cpu.pc == stop:
            break
except NeedKey:
    pass
out['marks'] = st['marks']
out['end_cycle'] = mc.cpu.cycles - st['t0']
json.dump(out, open('scratch/prompts_%s.json' % NAME, 'w'))
print(NAME, 'pressed', st['pressed'], 'marks', st['marks'], 'end', out['end_cycle'])
