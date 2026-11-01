"""TASK-031c: record every drawing call of the three intermission cartoons on the oracle (development tool; original data from scratch/).
Hidden menu: Ctrl-V Ctrl-C, 'C', cartoon number. Records, per call: [cycle, routine, A, Y (table base for the blitters), column $04, row $10,
width $1A, row count $18, $60, $61, $70, $71, $66 (Pac-Man/ghost working registers), caller, pixel x $0C/$0D]. Writes scratch/cartoon_<n>.json.
Usage: python -I tools/trace_cartoon.py [1|2|3 ...]   (about 2 minutes each; stops itself after 8 minutes)"""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from cov_close import factory
import explore

ENTRIES = {0x6A20: 'dispatch', 0x55A6: 'or_blit', 0x69A1: 'masked_clear', 0x696A: 'box_clear', 0x693A: 'tile', 0x6172: 'dot', 0x6EE2: 'image12x3',
           0x8259: 'big_sprite', 0x6F46: 'page_clear', 0x6F2F: 'hires_on', 0x7D80: 'w7D80', 0x7D8D: 'w7D8D', 0x7D9A: 'w7D9A', 0x7DA9: 'w7DA9', 0x699E: 'w699E'}


DRAWS = ('page_clear', 'hires_on', 'masked_clear', 'dispatch', 'big_sprite', 'box_clear', 'tile', 'dot', 'image12x3')


def chain(ev):
    """SHA-256 chain over the drawing calls only (routine, A, Y, pixel x, row, caller-independent); timing (cycles) is not part of it."""
    import hashlib
    h = b''
    for e in ev:
        if e[1] in DRAWS:
            h = hashlib.sha256(h + repr((e[1], e[2], e[3], e[14], e[5], e[8], e[9], e[10], e[11], e[12])).encode()).digest()
    return h.hex()


def run_one(n):
    t0 = time.monotonic()
    mc = factory()
    ev = []
    step = mc.cpu.step

    def hook():
        pc = mc.cpu.pc
        if pc in ENTRIES:
            c, m = mc.cpu, mc.m
            sp = c.sp
            caller = ((m[0x100 + ((sp + 2) & 0xFF)] << 8 | m[0x100 + ((sp + 1) & 0xFF)]) - 2) & 0xFFFF
            ev.append([c.cycles, ENTRIES[pc], c.a, c.y, m[0x04], m[0x10], m[0x1A], m[0x18], m[0x60], m[0x61], m[0x70], m[0x71], m[0x66], caller, m[0x0C] | m[0x0D] << 8])
        return step()
    mc.cpu.step = hook
    explore.to_players_prompt(mc)
    explore.hidden_menu(mc, 'C')
    explore.until_key(mc, 5_000_000)
    start = len(ev)
    c0 = mc.cpu.cycles
    explore.send(mc, [str(n), '\r'])
    ok = explore.until_key(mc, 400_000_000)
    ev = ev[start:]
    out = {'cartoon': n, 'cycles': mc.cpu.cycles - c0, 'returned_to_key_wait': ok, 'clicks': len(mc.speaker), 'events': ev,
           'final_page_sha256': __import__('hashlib').sha256(bytes(mc.m[0x2000:0x4000])).hexdigest()}
    json.dump(out, open('scratch/cartoon_%d.json' % n, 'w'))
    out['draw_chain'] = chain(ev)
    json.dump(out, open('scratch/cartoon_%d.json' % n, 'w'))
    names = {}
    for e in ev:
        names[e[1]] = names.get(e[1], 0) + 1
    run_one.last = out
    print('cartoon', n, 'cycles', out['cycles'], 'events', len(ev), names, 'clicks', out['clicks'], 'ok', ok, '%.0f s' % (time.monotonic() - t0), flush=True)


if __name__ == '__main__':
    for n in ([int(a) for a in sys.argv[1:]] or [1, 2, 3]):
        run_one(n)
