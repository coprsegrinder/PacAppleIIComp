"""TASK-027c: hi-res screen dump and golden-hash tool for the headless oracle (development tool; original data from scratch/).
  hires_page(mc)  -> the 8,192 bytes of hi-res page 1 ($2000-$3FFF)
  python -I tools/dump_hires.py golden [out.json]   replay the fixed scenario and write the SHA-256 of the page at each checkpoint
  python -I tools/dump_hires.py png <instructions> <out.png>   (see render_screen.py for a scaled picture)
Scenario (deterministic): boot, '1' + Return, attract in steps of 5,000,000 instructions up to 70,000,000, then a game with keys chosen by
random.Random(7) (one key with probability 1/2 before each 150,000-instruction block); checkpoints after blocks 50, 100, ... 400.
Only hashes are stored (tests/golden/hires_hashes.json), never the pictures. Stops itself after 15 minutes."""
import hashlib, json, os, random, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory

START = time.monotonic()


def hires_page(mc):
    return bytes(mc.m[0x2000:0x4000])


def digest(mc):
    return hashlib.sha256(hires_page(mc)).hexdigest()


def run_golden():
    mc = factory()
    out = []
    for st in ('key:1', 'key:cr'):
        do_step(mc, st, lambda *_: None)
    for i in range(1, 15):
        try:
            mc.run(5_000_000)
        except NeedKey:
            mc.push_key(13)
        out.append({'label': 'attract %d M' % (5 * i), 'sha256': digest(mc), 'switches': dict(mc.switches)})
    start_game(mc)
    rng = random.Random(7)
    for blk in range(1, 401):
        if time.monotonic() - START > 15 * 60:
            raise SystemExit('time limit reached')
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)
        if blk % 50 == 0:
            out.append({'label': 'game block %d' % blk, 'sha256': digest(mc), 'switches': dict(mc.switches), 'level': mc.m[0x55], 'score_bytes': mc.m[0x5A:0x5D].hex()})
    return out


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'golden':
        res = run_golden()
        path = sys.argv[2] if len(sys.argv) > 2 else 'tests/golden/hires_hashes.json'
        json.dump(res, open(path, 'w'), indent=1)
        for r in res:
            print(r['label'], r['sha256'][:16], r['switches'])
    else:
        raise SystemExit(__doc__)
