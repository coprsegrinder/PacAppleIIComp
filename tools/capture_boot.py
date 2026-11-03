"""TASK-033-4f: the boot menus of the original ($4027 -> JSR $4821: "CHOOSE A MODE (1 - 2)", "NORMAL OR CUSTOMIZED KEYBOARD (N/C)?", the four key prompts) driven with a key list, with
the real monitor ROM (development tool; original data from scratch/). Cycles count from the first instruction of the program ($4027).
Usage: python -I tools/capture_boot.py <name> <key> [<key> ...]      keys: sp, cr, esc, or single characters (a token longer than one character is typed character by character).
Writes scratch/boot_<name>.json: after every key wait (the loop at $FD21) the cycle, text page 2, zero page, $F5, the four key constants $6FBA/$6FB6/$6FB2/$6FBE, and at the end the cycle of
$52BB (the first attract turn) and of the marks passed on the way."""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
from apple2 import NeedKey

NAME, TOKENS = sys.argv[1], sys.argv[2:]
NAMES = {'sp': 0x20, 'cr': 0x0D, 'esc': 0x1B}
mc = make_machine(real_rom=True)
assert mc.cpu.pc == 0x4027
base = mc.cpu.cycles
MARKS = {0x402E, 0x405E, 0x4061, 0x48A6, 0x492F, 0x4955, 0x4A00, 0x43D2, 0x52BB}
marks = []
step = mc.cpu.step


def hook():
    if mc.cpu.pc in MARKS:
        marks.append([mc.cpu.pc, mc.cpu.cycles - base])
    return step()


mc.cpu.step = hook
keys = []
for t in TOKENS:
    if t in NAMES:
        keys.append(NAMES[t])
    else:
        keys.extend(ord(ch) for ch in t)
waits = []


def run():
    try:
        for _ in range(2000):
            mc.run(50_000, stop_pc=0x52BB)
            if mc.cpu.pc == 0x52BB:
                return
    except NeedKey:
        pass


def snap(key):
    m = mc.m
    waits.append({'key': key, 'cycle': mc.cpu.cycles - base, 'page': bytes(m[0x800:0xC00]).hex(), 'zp': bytes(m[0:256]).hex(), 'f5': m[0xF5],
                  'keys': [m[0x6FBA], m[0x6FB6], m[0x6FB2], m[0x6FBE]], 'marks': list(marks)})
    marks.clear()


run()
snap(None)
for k in keys:
    mc.push_key(k)
    run()
    snap(k)
    if mc.cpu.pc == 0x52BB:
        break
waits[-1]['t0'] = mc.cpu.cycles - base if mc.cpu.pc == 0x52BB else None
json.dump(waits, open('scratch/boot_%s.json' % NAME, 'w'))
print(NAME, [(w['key'], w['cycle'], w['f5']) for w in waits], 't0', waits[-1].get('t0'))
