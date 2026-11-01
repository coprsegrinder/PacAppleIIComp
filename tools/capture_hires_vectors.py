"""TASK-027 part 2: capture real drawing calls of the original as test vectors for the C++ hires layer (development tool; original data from scratch/).
Writes scratch/hires_vectors.bin (LOCAL ONLY: it contains pages drawn from original data; it is git-ignored and must never be committed).
Format: 'HIRV', u32 count, then per record: u8 type, 8 x i32 parameters, 8192 bytes page before, 8192 bytes page after.
Types (parameters p0..p7):
 1 dispatch ($6A20):      base, x, y, flag1D, flag1E, 0, 0, 0       (3 x 10 image, 7 shifted copies at base; OR path if $1D and $1E are both nonzero)
 2 blit_or ($55A6):       data address ($55B2/3 + $03), column, row, width, rows, 0, 0, 0
 3 masked_clear ($69A1):  mask table address, x, y, 0, 0, 0, 0, 0
 4 box_clear ($696A):     $50, 0, ...
 5 small_dot ($6172):     x, y
 6 tile ($693A):          table address, x, y
 7 image12x3 ($6EE2):     data address, column, row
 8 big_sprite ($8259):    frame address, x, y
 9 shifted_image ($7E1F): image pointer, x, y, flag1D, flag1E
Scenarios: play (boot, attract, a game with random keys) and the cartoons 1 and 3 (hidden menu). Each type keeps up to 150 distinct vectors.
Usage: python -I tools/capture_hires_vectors.py   (stops itself after 15 minutes)"""
import os, random, struct, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import NeedKey
from autoplay import start_game, KEYSET
from play import do_step
from cov_close import factory
import explore

START = time.monotonic()
LIMIT = 150
ENTRY = {0x6A20: 1, 0x55A6: 2, 0x69A1: 3, 0x696A: 4, 0x6172: 5, 0x693A: 6, 0x6EE2: 7, 0x8259: 8, 0x7E1F: 9}


class Collector:
    def __init__(self, mc):
        self.mc = mc
        self.records = []
        self.seen = {t: set() for t in range(1, 10)}
        self.count = {t: 0 for t in range(1, 10)}
        self.pending = []                      # (return pc, record)
        self.last = 0

    def params(self, t):
        m, c = self.mc.m, self.mc.cpu
        x = m[0x0C] | m[0x0D] << 8
        if t == 1:
            return [c.a | c.y << 8, x, m[0x10], m[0x1D], m[0x1E], 0, 0, 0]
        if t == 2:
            return [(m[0x55B2] | m[0x55B3] << 8) + m[0x03], m[0x04], m[0x10], m[0x1A], m[0x18], 0, 0, 0]
        if t == 3:
            return [m[0x69B8] | m[0x69B9] << 8, x, m[0x10], 0, 0, 0, 0, 0]
        if t == 4:
            return [m[0x50], 0, 0, 0, 0, 0, 0, 0]
        if t == 5:
            return [x, m[0x10], 0, 0, 0, 0, 0, 0]
        if t == 6:
            return [m[0x6951] | m[0x6952] << 8, x, m[0x10], 0, 0, 0, 0, 0]
        if t == 7:
            return [c.a | c.y << 8, m[0x04], m[0x10], 0, 0, 0, 0, 0]
        if t == 8:
            return [0, m[0x60], m[0x61], 0, 0, 0, 0, 0]
        return [m[0x16] | m[0x17] << 8, x, m[0x10], m[0x1D], m[0x1E], 0, 0, 0]

    def hook_factory(self):
        mc = self.mc
        step = mc.cpu.step

        def hook():
            pc, c, m = mc.cpu.pc, mc.cpu, mc.m
            if self.pending:
                for k, (rpc, rsp, rrec) in enumerate(self.pending):
                    if pc == rpc and c.sp == rsp:               # the matching return (same address and same stack depth, so nested calls cannot be mixed up)
                        del self.pending[k]
                        rrec[3] = bytes(m[0x2000:0x4000])
                        self.records.append(rrec)
                        break
            if pc == 0x826D and self.records is not None:                 # big sprite: the frame pointer is patched in by now
                for _, _, rec in self.pending:
                    if rec[0] == 8 and rec[1][0] == 0:
                        rec[1][0] = m[0x827B] | m[0x827C] << 8
                        rec[1][1], rec[1][2] = m[0x60], m[0x61]
            t = ENTRY.get(pc)
            if t == 9 and not (m[self.last] == 0x20 and m[self.last + 1] == 0x1F and m[self.last + 2] == 0x7E):
                t = None                                        # $7E1F is also reached by falling through from the ghost-eaten routine: the 'return' is then a different one
            if t and not (pc == 0x55A6 and self.last in (0x55C4, 0x6A79)) and len(self.pending) < 12:
                self.last = pc
                if self.count[t] < LIMIT * 40:
                    p = self.params(t)
                    key = tuple(p) if t not in (1, 9) else (p[0], p[1] % 7, p[2] & 1, p[3], p[4])
                    if t == 8:
                        key = (m[0x66] & 3, m[0x60] % 7)
                    if key not in self.seen[t] and len(self.seen[t]) < LIMIT:
                        self.seen[t].add(key)
                        sp = c.sp
                        ret = (((m[0x100 + ((sp + 2) & 0xFF)] << 8 | m[0x100 + ((sp + 1) & 0xFF)]) + 1) & 0xFFFF)
                        self.pending.append((ret, (sp + 2) & 0xFF, [t, p, bytes(m[0x2000:0x4000]), None]))
                    self.count[t] += 1
            self.last = pc
            return step()
        return hook

    def save(self, path):
        with open(path, 'wb') as f:
            f.write(b'HIRV' + struct.pack('<I', len(self.records)))
            for t, p, before, after in self.records:
                f.write(struct.pack('<B8i', t, *p) + before + after)


def play(col, minutes):
    mc = col.mc
    for st in ('key:1', 'key:cr', '70_000_000'):
        do_step(mc, st, lambda *_: None)
    start_game(mc)
    rng = random.Random(3)
    t0 = time.monotonic()
    for _ in range(600):
        if time.monotonic() - t0 > minutes * 60:
            break
        if rng.random() < .5:
            mc.push_key(rng.choice(KEYSET))
        try:
            mc.run(150_000)
        except NeedKey:
            mc.push_key(13)


def cartoon(col, n):
    mc = col.mc
    explore.to_players_prompt(mc)
    explore.hidden_menu(mc, 'C')
    explore.until_key(mc, 5_000_000)
    explore.send(mc, [str(n), '\r'])
    explore.until_key(mc, 400_000_000)


def main():
    out = 'scratch/hires_vectors.bin'
    allrec = []
    for name, fn in (('play', lambda c: play(c, 4)), ('cartoon1', lambda c: cartoon(c, 1)), ('cartoon3', lambda c: cartoon(c, 3))):
        if time.monotonic() - START > 14 * 60:
            break
        mc = factory()
        col = Collector(mc)
        mc.cpu.step = col.hook_factory()
        fn(col)
        allrec += col.records
        print(name, 'records', len(col.records), {t: len(s) for t, s in col.seen.items() if s}, '%.0f s' % (time.monotonic() - START), flush=True)
    merged = Collector(factory())
    merged.records = allrec
    merged.save(out)
    by = {}
    for r in allrec:
        by[r[0]] = by.get(r[0], 0) + 1
    print('wrote', out, len(allrec), 'records by type', by, 'size %.1f MB' % (os.path.getsize(out) / 1e6))


if __name__ == '__main__':
    main()
