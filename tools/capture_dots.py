"""TASK-028d: capture real runs of the dot-drawing routine $6113 (walks the maze map at $623A, draws small dots and energizer tiles) as test vectors.
Writes scratch/dots_vectors.bin (LOCAL ONLY: pages drawn from original data; git-ignored, never commit).
Format: 'DOTV', u32 count, then per record: 896 bytes map ($623A), 8192 bytes page before, 8192 bytes page after.
Usage: python -I tools/capture_dots.py   (attract demo + a short random game; stops itself after 4 minutes)"""
import os, random, struct, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory

START = time.monotonic()


def main():
    mc = factory()
    recs, pending, seen = [], [], set()
    step = mc.cpu.step

    def hook():
        pc, m = mc.cpu.pc, mc.m
        if pc == 0x6113 and len(recs) + len(pending) < 12:
            key = bytes(m[0x623A:0x623A + 896])
            if key not in seen:
                seen.add(key)
                pending.append([key, bytes(m[0x2000:0x4000])])
        elif pc == 0x6171 and pending:
            rec = pending.pop()
            recs.append((rec[0], rec[1], bytes(m[0x2000:0x4000])))
        return step()
    mc.cpu.step = hook
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    rng = random.Random(5)
    for _ in range(600):
        if time.monotonic() - START > 240:
            break
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
    with open('scratch/dots_vectors.bin', 'wb') as f:
        f.write(b'DOTV' + struct.pack('<I', len(recs)))
        for mp, b, a in recs:
            f.write(mp + b + a)
    print(len(recs), 'records;', 'distinct maps', len(seen), 'elapsed %.0f s' % (time.monotonic() - START))


if __name__ == '__main__':
    main()
