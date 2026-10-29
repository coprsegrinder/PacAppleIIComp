"""Trace the ghost modes during play (development tool; uses original data from scratch/).
Watches: $E9 (corner/chase phase), $E4 (frightened active), the per-ghost flags $78/$79 (through the ghost records), and counts
  mode ticks   = entries to $808C (the scatter/chase countdown, called from the main loop at $7874)
  ghost steps  = entries to $8763 (the per-ghost step timer)
  fright ticks = executions of $8772 (the 16-bit frightened countdown being decremented)
Usage: python -I tools/trace_modes.py [instructions_before_energizer] [instructions_after]
At the chosen moment the game's own energizer routine $6D60 is called on the oracle (a JSR is simulated), as if an energizer had been eaten.
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from play import make_machine, do_step
from autoplay import start_game

WATCH = {'E9': 0xE9, 'E4': 0xE4}


def simulate_jsr(mc, address):
    """Make the CPU behave as if the next instruction were `JSR address` (call at an instruction boundary)."""
    cpu = mc.cpu
    ret = (cpu.pc - 1) & 0xFFFF
    cpu.push(ret >> 8)
    cpu.push(ret & 0xFF)
    cpu.pc = address


def install(mc):
    """Hook cpu.step. Returns (events, counters): events = [{'cycle','var','old','new','pc','mode_ticks'}]."""
    events = []
    counters = {'mode_ticks': 0, 'ghost_steps': 0, 'fright_ticks': 0}
    last = {name: mc.m[a] for name, a in WATCH.items()}
    step = mc.cpu.step

    def hooked():
        pc = mc.cpu.pc
        if pc == 0x808C:
            counters['mode_ticks'] += 1
        elif pc == 0x8763:
            counters['ghost_steps'] += 1
        elif pc == 0x8772:
            counters['fright_ticks'] += 1
        cycles = step()
        for name, a in WATCH.items():
            v = mc.m[a]
            if v != last[name]:
                events.append({'cycle': mc.cpu.cycles, 'var': name, 'old': last[name], 'new': v, 'pc': pc, 'mode_ticks': counters['mode_ticks']})
                last[name] = v
        return cycles
    mc.cpu.step = hooked
    return events, counters


def ghost_flags(mc):
    """($78 frightened, $79 flag) of each ghost, read from the four 20-byte records at $84 ($78 = record offset 8, $79 = offset 9)."""
    return [(mc.m[0x84 + 20 * g + 8], mc.m[0x84 + 20 * g + 9]) for g in range(4)]


def main():
    before = int(sys.argv[1].replace('_', '')) if len(sys.argv) > 1 else 6_000_000
    after = int(sys.argv[2].replace('_', '')) if len(sys.argv) > 2 else 6_000_000
    mc = make_machine()
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    events, counters = install(mc)
    start_game(mc)
    t0 = mc.cpu.cycles
    print('game started: $E9=%d $E4=%d level $55=%d' % (mc.m[0xE9], mc.m[0xE4], mc.m[0x55]))
    mc.run(before)
    mc.run(10_000_000, stop_pc=0x530F)                       # finish the current pass: inject between passes, not in the middle of a ghost update
    print('before energizer: mode_ticks=%d $E9=%d $E4=%d ghost flags(78,79)=%s' % (counters['mode_ticks'], mc.m[0xE9], mc.m[0xE4], ghost_flags(mc)))
    simulate_jsr(mc, 0x6D60)
    mc.run(10_000, stop_pc=0x530F)                          # run the routine until it returns to the injection point
    print('right after $6D60: $E9=%d $E4=%d timer $E1/$E2=%d ghost flags=%s $EA/$EB=%d' % (
        mc.m[0xE9], mc.m[0xE4], mc.m[0xE1] | mc.m[0xE2] << 8, ghost_flags(mc), mc.m[0xEA] | mc.m[0xEB] << 8))
    ticks_at_start = counters['fright_ticks']
    try:
        mc.run(after // 2)
        print('mid-frightened: $E4=%d timer $E1/$E2=%d ghost flags=%s' % (mc.m[0xE4], mc.m[0xE1] | mc.m[0xE2] << 8, ghost_flags(mc)))
        mc.run(after - after // 2)
    except NeedKey:
        pass
    print('after: $E4=%d fright_ticks used=%d ghost flags=%s game over flag $E8=%d' % (mc.m[0xE4], counters['fright_ticks'] - ticks_at_start, ghost_flags(mc), mc.m[0xE8]))
    print('transitions (cycle since game start, var old->new, writer pc, mode_ticks so far):')
    for e in events[:40]:
        print('  %9d  %s %d->%d  pc=%04X  mode_ticks=%d' % (e['cycle'] - t0, e['var'], e['old'], e['new'], e['pc'], e['mode_ticks']))
    print('totals: mode_ticks=%d ghost_steps=%d fright_ticks=%d over %d cycles' % (counters['mode_ticks'], counters['ghost_steps'], counters['fright_ticks'], mc.cpu.cycles - t0))


if __name__ == '__main__':
    main()
