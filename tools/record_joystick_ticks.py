"""TASK-034-PADDLE: record the complete logical state at EVERY tick of a game played in JOYSTICK mode (mode 2 of the mode screen) on the oracle (development tool; original data from scratch/).
Same file format as tools/record_ticks.py (scratch/ticks_JOY.bin), plus a side file scratch/ticks_JOY.bin.pad with two bytes per tick: the positions of paddle 0 and paddle 1 (0..255) that the
original's $701E / $7041 read in that tick. The stick moves like a player's: it is held at a position for 4-40 ticks, positions are the ends, the middle or something in between.
Usage: python -I tools/record_joystick_ticks.py [ticks] [seed]      (a few minutes)"""
import hashlib, os, random, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game
from play import do_step
from cov_close import factory
from golden_trace import state_bytes, TICK_PC

STATE_SIZE = 1010
POSITIONS = [0, 0, 255, 255, 127, 127, 127, 40, 66, 67, 68, 100, 165, 166, 167, 200]


def main(ticks=8000, seed=11):
    rng = random.Random(seed)
    mc = factory()
    for st in ('key:2', 'key:cr', '70_000_000'):      # joystick mode
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    states, pads = [], bytearray()
    hold = {'left': 0, 'x': 127, 'y': 127}
    done = {'v': False}
    step = mc.cpu.step

    def hook():
        if mc.cpu.pc == TICK_PC and mc.m[0xE8] == 0 and not done['v']:
            states.append(state_bytes(mc.m))
            if len(states) > ticks:
                done['v'] = True
            else:
                if hold['left'] <= 0:
                    hold['left'] = rng.randint(4, 40)
                    hold['x'] = rng.choice(POSITIONS)
                    hold['y'] = rng.choice(POSITIONS) if rng.random() < 0.35 else 127   # mostly one axis at a time, like a stick pushed along a corridor
                    if rng.random() < 0.5:
                        hold['x'], hold['y'] = hold['y'], hold['x']
                hold['left'] -= 1
                mc.paddles[0], mc.paddles[1] = hold['x'], hold['y']
                pads.extend((hold['x'], hold['y']))
        return step()
    mc.cpu.step = hook
    guard = 0
    while not done['v'] and guard < 4000:
        guard += 1
        if mc.m[0xE8] != 0 and states:
            break                                      # game over: the demo started again
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    if len(states) > len(pads) // 2:
        states = states[:len(pads) // 2 + 1]
    h = b''
    for s in states:
        assert len(s) == STATE_SIZE
        h = hashlib.sha256(h + s).digest()
    with open('scratch/ticks_JOY.bin', 'wb') as f:
        f.write(b'TICK' + struct.pack('<II', len(states), STATE_SIZE) + h)
        for s in states:
            f.write(s)
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<I', 0))
    open('scratch/ticks_JOY.bin.pad', 'wb').write(bytes(pads))
    print('J: ticks recorded', len(states), 'paddle bytes', len(pads), 'game over reached:', mc.m[0xE8] != 0, flush=True)


if __name__ == '__main__':
    main(*(int(a) for a in sys.argv[1:]))
