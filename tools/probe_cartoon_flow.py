"""TASK-031f: two open questions about the intermissions, answered on the oracle (development tool; original data from scratch/).
  key  : a key pressed DURING a cartoon in a real game (level 2 completed by a poke): is it seen afterwards? Logs every $C000/$C010 access after the cartoon
         returns, the keyboard latch at the return, and compares the state 400 ticks after the cartoon with a control run without the key.
  two  : a 2-player game in which levels are ended by pokes: which player gets a cartoon, and at which level? Logs each entry of $8155 ($50 = current player, $55 = level).
Usage: python -I tools/probe_cartoon_flow.py key|two   (stops itself after 9 minutes)"""
import hashlib, os, random, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import KEYSET
from play import do_step
from cov_close import factory

START = time.monotonic()
TICK = 0x7861


def new_game(players):
    mc = factory()
    for st in ('key:1', 'key:cr', '70_000_000', 'key:sp', '3_000_000', 'key:%d' % players, '3_000_000', 'key:cr', '1_000_000'):
        do_step(mc, st, lambda *_: None)
    return mc


def logical(mc):
    return hashlib.sha256(bytes(mc.m[0x52:0x70]) + bytes(mc.m[0x84:0xD4]) + bytes(mc.m[0x623A:0x65BA])).hexdigest()[:16]


def run_key(press):
    mc = new_game(1)
    log, st = [], {'tick': 0, 'wait': 0, 'cartoon_at': None, 'after': None, 'ret': None, 'pressed': False}
    step, io = mc.cpu.step, mc.io

    def io_hook(a, v):
        if st['ret'] is not None and a in (0xC000, 0xC010) and len(log) < 12:
            log.append(('$%04X' % a, 'pc $%04X' % mc.cur_pc, 'kbd before $%02X' % mc.kbd, 'tick %d' % (st['tick'] - st['ret'])))
        return io(a, v)
    mc.io = io_hook

    def hook():
        pc = mc.cpu.pc
        if pc == 0x81AE and st['cartoon_at'] is None:
            st['cartoon_at'] = st['tick']
            if press:
                mc.kbd = ord(KEYSET[0]) | 0x80         # a key during the cartoon: the real keyboard latches it at once (the oracle's queue only latches on a read)
                st['pressed'] = True
        if pc == 0x6D10 and st['ret'] is None and st['cartoon_at'] is not None:
            st['ret'] = st['tick']
            st['kbd_at_return'] = (mc.kbd, list(mc.keys))
        if pc == TICK and mc.m[0xE8] == 0:
            t = st['tick']
            if mc.m[0x55] in (1, 2) and st['cartoon_at'] is None:
                st['wait'] += 1
                if st['wait'] >= 300:
                    mc.m[0x56] = 3
                    st['wait'] = 0
            if st['ret'] is not None and t == st['ret'] + 400:
                st['after'] = (logical(mc), mc.m[0x63], mc.m[0x62], mc.m[0x60], mc.m[0x61])
            st['tick'] = t + 1
        return step()
    mc.cpu.step = hook
    rng = random.Random(9)                               # the same random keys in both runs
    for _ in range(30000):
        if st['after'] or time.monotonic() - START > 9 * 60 or mc.m[0xE8] != 0:
            break
        k = rng.choice(KEYSET)
        if rng.random() < .5 and not mc.keys and not (st['cartoon_at'] is not None and st['ret'] is None):
            mc.push_key(k)                                # random keys only outside the cartoon and never queued up (the real keyboard has one latch)
        try:
            mc.run(20_000)
        except NeedKey:
            mc.push_key(13)
    return st, log


def probe_key():
    global START
    for press in (False, True):
        START = time.monotonic()
        st, log = run_key(press)
        print('--- key pressed during the cartoon:', press)
        print(' cartoon started at tick', st['cartoon_at'], '| returned at tick', st['ret'], '| keyboard latch and queue at the return:', st.get('kbd_at_return'))
        for l in log:
            print('   ', l)
        print(' state 400 ticks after the return (hash, $63 requested dir, $62 dir, pac x, pac y):', st['after'], flush=True)


def probe_two():
    mc = new_game(2)
    seen, st = [], {'tick': 0, 'wait': 0}
    step = mc.cpu.step

    def hook():
        pc = mc.cpu.pc
        if pc in (0x81AE, 0x83D0, 0x8486):
            seen.append((st['tick'], 'CARTOON STARTS at $%04X for player $50 = %d' % (pc, mc.m[0x50]), 'level $55 = %d' % mc.m[0x55], ''))
        if pc == 0x8155:
            seen.append((st['tick'], 'player $50 = %d' % mc.m[0x50], 'level $55 = %d' % mc.m[0x55], 'lives $58 = $%02X' % mc.m[0x58]))
        if pc == TICK and mc.m[0xE8] == 0:
            if mc.m[0x55] in (1, 2, 3):
                st['wait'] += 1
                if st['wait'] >= 250 and mc.m[0x56] > 3:
                    mc.m[0x56] = 3
                    st['wait'] = 0
            st['tick'] += 1
        return step()
    mc.cpu.step = hook
    rng = random.Random(4)
    switches, last = [], mc.m[0x50]
    for _ in range(6000):
        if len(seen) >= 8 or time.monotonic() - START > 9 * 60 or mc.m[0xE8] != 0:
            break
        if mc.m[0x50] != last:
            switches.append((st['tick'], last, mc.m[0x50]))
            last = mc.m[0x50]
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    print('entries of $8155 (tick, player, level, lives):')
    for s in seen:
        print('  ', s)
    print('player switches (tick, from, to):', switches[:10], '| ticks', st['tick'], '| E8', mc.m[0xE8], '| elapsed %.0f s' % (time.monotonic() - START))
    return seen


if __name__ == '__main__':
    {'key': probe_key, 'two': probe_two}[sys.argv[1]]()
