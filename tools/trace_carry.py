"""TASK-021c: the six `INC $17` carry fix-ups at $5754..$5825 add to the pointer $16/$17 that starts at $625D. This records, for every
execution of the six `ADC #$14` sites (and the two ADC #$23 sites), the pointer value, to show whether a carry can ever happen.
Development tool (needs scratch/). Stops itself after 8 minutes. Usage: python -I tools/trace_carry.py"""
import os, sys, time, random, json
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory

SITES = {0x576A: 'a', 0x57A4: 'b', 0x57E5: 'c', 0x581F: 'd'}   # ADC #$14 operands: the instruction address (found below)
START = time.monotonic()


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    adc14 = [0x4000 + i for i in range(len(body) - 6)
             if body[i:i + 2] == bytes([0xA5, 0x16]) and body[i + 2:i + 4] == bytes([0x69, 0x14]) and body[i + 4:i + 6] == bytes([0x85, 0x16])]
    print('LDA $16 / ADC #$14 / STA $16 sites:', [hex(a) for a in adc14])
    seen = {a: set() for a in adc14}
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    step = mc.cpu.step
    rng = random.Random(3)

    def hook():
        pc = mc.cpu.pc
        if pc in seen:
            seen[pc].add((mc.m[0x16], mc.m[0x17]))
        return step()
    mc.cpu.step = hook
    for _ in range(300):
        if time.monotonic() - START > 8 * 60:
            break
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    for a in adc14:
        print(hex(a), sorted('%02X%02X' % (h, l) for l, h in seen[a]))
    json.dump({hex(a): sorted(seen[a]) for a in adc14}, open('scratch/trace_carry.json', 'w'))
    print('elapsed %.0f s' % (time.monotonic() - START))


if __name__ == '__main__':
    main()
