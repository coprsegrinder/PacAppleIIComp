"""TASK-021d: execute the last unexecuted bytes by FORCING the state they need (development tool; original data from scratch/).
Dead code stays dead in the real game; this only shows that the bytes are valid code that runs, and what they do, when the guarded
condition is forced. Forced conditions (hooks on the instruction before the guard):
  carry flag set at the six `BCC +2 / INC $17` fix-ups ($5752 $576E $57A8 $57CD $57E9 $5823)
  zero flag cleared at the always-taken BEQs ($6DEC, $8A38) and set at the always-taken BNE ($53B0)
  $0C = $FF at $5D34 (wrap into $5D38), $1D = 0 at $5D0B and $5D57 (the alternative draw loops), $19 = 0 at $56E5 (no lives to draw)
Writes scratch/cov_close6.json; stops itself after 12 minutes. Usage: python -I tools/cov_close6.py"""
import os, sys, time, random
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory
from cov_close5 import save

C, Z = 0x01, 0x02
TARGETS = [0x5754, 0x5770, 0x57AA, 0x57CF, 0x57EB, 0x5825, 0x5D31, 0x5D66, 0x56E9, 0x6DEE, 0x8A3A, 0x53B2]
START = time.monotonic()


def run_at(mc, addr, steps, hits, forced):
    """Stage 2: call the routine that contains a guarded byte directly (return address = a stop address that is never executed)."""
    c = mc.cpu
    c.push(0x40); c.push(0x26)                      # RTS returns to $4027 (the entry point), where run() stops
    c.pc = addr
    try:
        mc.run(steps, stop_pc=0x4027)
    except Exception as e:                          # the bytes behind a forced always-taken branch are data
        print('   stopped at $%04X: %s' % (c.pc, type(e).__name__))


def main():
    mc = factory()
    hits = {t: 0 for t in TARGETS}
    forced = {}
    step = mc.cpu.step
    set_c = (0x5752, 0x576E, 0x57A8, 0x57CD, 0x57E9, 0x5823)
    clr_z, set_z = (0x6DEC, 0x8A38), (0x53B0,)
    stage = {'two': False}

    def hook():
        c, pc = mc.cpu, mc.cpu.pc
        if pc in hits:
            hits[pc] += 1
        if mc.m[0xE8] == 0 or stage['two']:
            n = forced.get(pc, 0)
            if pc in set_c and n < 3:
                c.p |= C; forced[pc] = n + 1
            elif pc in clr_z and n < 3:
                c.p &= ~Z; forced[pc] = n + 1
            elif pc in set_z and n < 3 and stage['two']:
                c.p |= Z; forced[pc] = n + 1
            elif pc in (0x5D0B, 0x5D57) and n < 2:
                mc.m[0x1D] = 0; forced[pc] = n + 1
            elif pc == 0x5D34 and n < 1 and stage['two']:
                mc.m[0x0C] = 0xFF; forced[pc] = n + 1     # INC $0C wraps to 0: the INC $0D path
            elif pc == 0x56E5 and n < 1:
                mc.m[0x19] = 0; forced[pc] = n + 1
        return step()
    mc.cpu.step = hook
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    rng = random.Random(5)
    for _ in range(400):
        if time.monotonic() - START > 6 * 60 or all(hits[t] for t in (0x5754, 0x5770, 0x57AA, 0x57CF, 0x57EB, 0x5825, 0x8A3A)):
            break
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
        except Exception as e:                      # a forced carry can corrupt the picture or the program; keep the hits so far
            print('   stage 1 stopped:', e)
            break
    print('stage 1', {hex(k): v for k, v in hits.items()}, flush=True)
    stage['two'] = True
    mc.m[0xE8] = 0
    for addr, steps in ((0x5D53, 3_000_000), (0x56D9, 20_000), (0x6DE8, 50), (0x53A9, 40)):
        run_at(mc, addr, steps, hits, forced)
        print('stage 2 after $%04X' % addr, {hex(k): v for k, v in hits.items() if v}, flush=True)
    print('forced', {hex(k): v for k, v in forced.items()})
    save('6')
    print('elapsed %.0f s' % (time.monotonic() - START))


if __name__ == '__main__':
    main()
