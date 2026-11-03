"""TASK-033-4e2: the hidden menu of the original (Ctrl-V Ctrl-C at the players prompt) driven with a list of keys (development tool; original data from scratch/).
Usage: python -I tools/capture_hidden.py <name> <key> [<key> ...]      keys: sp, cr, ctrlv, ctrlc, ctrlx, bs, esc, or any single characters (a token longer than one character that is not
a name is typed character by character).   Space is latched at T0 + 100,000 first.   Writes scratch/hidden_<name>.json: after every key wait ($FD0C / $FD6F traps) the text page 2, the cursor ($24, $25),
$54E1, $55, the line buffer $0200-$021F, the cycle (from T0), and the cycles of the marked places since the previous wait."""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey
import explore

NAME, TOKENS = sys.argv[1], sys.argv[2:]
NAMES = {'sp': 0x20, 'cr': 0x0D, 'ctrlv': 0x16, 'ctrlc': 0x03, 'ctrlx': 0x18, 'bs': 0x08, 'esc': 0x1B}
MARKS = {0x4147, 0x4166, 0x4176, 0x4186, 0x4199, 0x419C, 0x419F, 0x41AF, 0x41B3, 0x41CC, 0x41D2, 0x41D9, 0x4144, 0x4217, 0x4272, 0x4279, 0x8155, 0x4233, 0x4163, 0x41AC}
mc = make_machine(real_rom=os.environ.get('A2ROM') == '1')   # A2ROM=1: the real Autostart Monitor ROM runs instead of the stand-ins (TASK-033-4e3)
step = mc.cpu.step
st = {'t0': None, 'pressed': False, 'marks': []}


def hook():
    c = mc.cpu
    if st['t0'] is None:
        if c.pc == 0x52BB and mc.m[0xE8] == 1:
            st['t0'] = c.cycles
    else:
        if not st['pressed'] and c.cycles - st['t0'] >= 100000:
            mc.push_key(0x20)
            mc._latch_key()
            st['pressed'] = True
        if c.pc in MARKS:
            st['marks'].append([c.pc, c.cycles - st['t0']])
    return step()


mc.cpu.step = hook
explore.send(mc, ['1', '\r'])
keys = []
for t in TOKENS:
    if t in NAMES:
        keys.append(NAMES[t])
    else:
        keys.extend(ord(ch) for ch in t)
waits = []


def run_to_wait():
    try:
        for _ in range(400):
            mc.run(100_000)
            if mc.cpu.pc == 0x52BB and waits:
                pass
    except NeedKey:
        pass


def snap(key):
    m = mc.m
    waits.append({'key': key, 'pc': mc.cpu.pc, 'cycle': mc.cpu.cycles - st['t0'], 'page': bytes(m[0x800:0xC00]).hex(), 'ch': m[0x24], 'cv': m[0x25], 'rank': m[0x54E1], 'm55': m[0x55],
                  'buf': bytes(m[0x200:0x220]).hex(), 'zp': bytes(m[0:256]).hex(), 'marks': st['marks'], 'getln': mc._getln is not None, 'text': mc.switches['text']})
    st['marks'] = []


run_to_wait()
snap(None)
for k in keys:
    mc.push_key(k)
    run_to_wait()
    snap(k)
json.dump(waits, open('scratch/hidden_%s.json' % NAME, 'w'))
print(NAME, [(hex(w['pc']), w['cycle']) for w in waits])
