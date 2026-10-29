"""Drive the rarely used paths of the game headless, with coverage (development tool; uses original data from scratch/).
Usage: python -I tools/explore.py <scenario> <out.json>
Scenarios:
  cartoons   hidden menu: at the players prompt press Ctrl-V, Ctrl-C, type C, then 1 / 2 / 3 (plays the three intermissions)
  rank       hidden menu: Ctrl-V, Ctrl-C, type R, rank 21, then play a game from that level with random keys
  custom     second menu 'C' (customised keyboard): choose four keys, then play and use them; also Esc pause
Each scenario starts from the boot. Prints what each step saw (prompt text, waits) and writes exec/read/write run lists.
"""
import json, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from trace_coverage import runs
from autoplay import play_one

CTRL_V, CTRL_C, ESC, CR = 0x16, 0x03, 0x1B, 0x0D


def text_page2(mc):
    out = []
    for r in range(24):
        b = 0x800 + (r % 8) * 0x80 + (r // 8) * 0x28
        line = ''.join(chr(c & 0x7F) if 0x20 <= (c & 0x7F) < 0x7F else '.' for c in mc.m[b:b + 40]).rstrip()
        if line:
            out.append(line)
    return out


def until_key(mc, limit=200_000_000):
    """Run until the game waits in RDKEY/GETLN (returns True) or `limit` instructions pass (False)."""
    try:
        mc.run(limit)
        return False
    except NeedKey:
        return True


def send(mc, keys):
    for k in keys:
        mc.push_key(k if isinstance(k, int) else ord(k))


def to_players_prompt(mc, mode='1', second='\r'):
    send(mc, [mode, second])
    mc.run(70_000_000)                       # attract loop
    send(mc, [' '])
    assert until_key(mc, 20_000_000), 'no players prompt'


def hidden_menu(mc, text):
    """At the players prompt: Ctrl-V, Ctrl-C, then GETLN text + Enter."""
    send(mc, [CTRL_V, CTRL_C])
    assert until_key(mc, 5_000_000), 'no GETLN prompt after Ctrl-V Ctrl-C'
    print('  prompt:', text_page2(mc))
    send(mc, list(text) + ['\r'])


def save(mc, out):
    c = mc.cov
    json.dump({'exec': runs(c['exec']), 'read': runs(c['read']), 'write': runs(c['write']),
               'opstart': [a for a in range(65536) if c['opstart'][a]], 'cycles': mc.cpu.cycles},
              open(out, 'w'))
    print('executed game-area bytes:', sum(c['exec'][0x4000:0xAA00]), '| unknown io', mc.unknown_io,
          '| rom calls', mc.rom_calls, '| speaker clicks', len(mc.speaker))


def scenario_cartoons(mc):
    to_players_prompt(mc)
    for n in '123':
        hidden_menu(mc, 'C')
        assert until_key(mc, 5_000_000)
        print('  prompt:', text_page2(mc))
        send(mc, [n, '\r'])
        ok = until_key(mc, 400_000_000)
        print('cartoon', n, '-> back at a key wait:', ok, '| page2 text:', text_page2(mc)[:2], '| clicks', len(mc.speaker))


def scenario_rank(mc):
    to_players_prompt(mc)
    hidden_menu(mc, 'R')
    assert until_key(mc, 5_000_000)
    print('  prompt:', text_page2(mc))
    send(mc, ['2', '1', '\r'])
    assert until_key(mc, 5_000_000)
    print('  after rank:', text_page2(mc))
    send(mc, ['1'])
    assert until_key(mc, 5_000_000)
    send(mc, ['\r'])
    mc.run(4_000_000)
    print('  start: $E8=%02X level var $54E1=%02X' % (mc.m[0xE8], mc.m[0x54E1]))
    rng = random.Random(7)
    n, over = play_one(mc, 1, rng, 150_000_000)
    print('  played %.1fM instr, game over: %s, clicks %d' % (n / 1e6, over, len(mc.speaker)))


def scenario_custom(mc):
    send(mc, ['1'])                                      # mode menu: keyboard
    assert until_key(mc, 80_000_000), 'no second menu'
    send(mc, ['C'])                                      # NORMAL OR CUSTOMIZED KEYBOARD? -> C
    for k, want in [('J', 'LEFT'), ('L', 'RIGHT'), ('I', 'UP'), ('M', 'DOWN')]:
        assert until_key(mc, 3_000_000), 'no key prompt before %s' % want
        print('  prompt:', [t for t in text_page2(mc) if 'MOVE' in t][-1:], '-> pressing', k)
        send(mc, [k])
    until_key(mc, 70_000_000)                            # attract loop (no key wait expected)
    send(mc, [' '])
    assert until_key(mc, 20_000_000)
    send(mc, ['1'])
    assert until_key(mc, 5_000_000)
    send(mc, [CR])
    mc.run(4_000_000)
    print('  game started: $E8=%02X patched compares $6FBA=%02X $6FB6=%02X $6FB2=%02X $6FBE=%02X' % (
        mc.m[0xE8], mc.m[0x6FBA], mc.m[0x6FB6], mc.m[0x6FB2], mc.m[0x6FBE]))
    for k in 'JLIM':
        send(mc, [k])
        mc.run(300_000)
        print('  key', k, '-> $63 =', mc.m[0x63])
        mc.run(1_500_000)
    send(mc, [ESC])
    mc.run(3_000_000)
    print('  after Esc: parked in the pause poll ($6F9A)?', mc.cpu.pc in range(0x6F9A, 0x6FA0), 'pc=%04X' % mc.cpu.pc)
    send(mc, ['X'])
    mc.run(3_000_000)
    print('  after another key: pc=%04X $E8=%02X' % (mc.cpu.pc, mc.m[0xE8]))


SCENARIOS = {'cartoons': scenario_cartoons, 'rank': scenario_rank, 'custom': scenario_custom}


def main():
    name, out = sys.argv[1], sys.argv[2]
    mc = make_machine(cov=True)
    SCENARIOS[name](mc)
    save(mc, out)


if __name__ == '__main__':
    main()
