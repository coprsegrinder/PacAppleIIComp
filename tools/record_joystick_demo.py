"""TASK-034-PADDLE: the attract-mode demo played in JOYSTICK mode (mode 2 of the mode screen), recorded like tools/record_special.py demo (development tool; original data from scratch/).
In joystick mode the original's step routine polls the input after each step of Pac-Man, and in the demo ($E8 <> 0) that poll writes $80 into $63. Writes scratch/joydemo/ticks_DEMO.bin,
scratch/joydemo/demo_stream.bin and an (all-zero) scratch/joydemo/ticks_DEMO.bin.pad that makes the game-core test run it in joystick mode (pass it with --joydemo).
Usage: python -I tools/record_joystick_demo.py"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
from record_special import write, TICK_PC
from apple2 import NeedKey
from play import do_step
from cov_close import factory


def main():
    os.makedirs('scratch/joydemo', exist_ok=True)
    mc = factory()
    for st in ('key:2', 'key:cr'):
        do_step(mc, st, lambda *_: None)
    states, extra, cycles = [], bytearray(), []
    step = mc.cpu.step

    def hook():
        if mc.cpu.pc == TICK_PC and mc.m[0xE8] != 0 and len(states) < 4000:
            states.append(G.state_bytes(mc.m))
            cycles.append(mc.cpu.cycles)
            lo, hi = mc.m[0x1B], mc.m[0x1C]
            p = ((hi << 8) | lo) + 1
            extra.extend([lo, hi, mc.m[p & 0xFFFF]])
        return step()
    mc.cpu.step = hook
    for _ in range(4000):
        if len(states) >= 4000:
            break
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    if len(states) < 1000:
        sys.exit('only %d demo ticks' % len(states))
    h = write('scratch/joydemo/ticks_DEMO.bin', states, [], [])
    open('scratch/joydemo/demo_stream.bin', 'wb').write(bytes(extra))
    with open('scratch/joydemo/demo_cycles.bin', 'wb') as f:      # as tools/record_demo_full.py: u32 count, then the cycle at which every tick starts
        f.write(struct.pack('<I', len(cycles)) + b''.join(struct.pack('<I', c) for c in cycles))
    open('scratch/joydemo/ticks_DEMO.bin.pad', 'wb').write(bytes(2 * len(states)))
    print('joystick demo ticks', len(states), 'chain', h.hex()[:16], flush=True)


if __name__ == '__main__':
    main()
