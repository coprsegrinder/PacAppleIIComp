"""TASK-030e4: input/output vectors of the 48 ghost decision routines run on the oracle (development tool; original data from scratch/).
For random inputs (half uniform, half near the ghost's own coordinates so that equalities are hit) the routine at each entry of the address table $8DBE is run to its RTS
with A, X, Y and the five memory bytes it can see ($70 $71 $72 $62 $69) set. Writes scratch/decide_vectors.bin: b'DECV', uint32 count, then 15 bytes per vector:
entry (uint16), A, X, Y, ghostX, ghostY, ghostDir, pacDir, pacTunnel, then the results A, ghostX, ghostY, ghostDir, and a flag byte (1 = reached RTS).
Usage: python -I tools/record_decide_vectors.py [vectors-per-routine]     (default 2000; about 1-2 minutes)"""
import os, random, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import play

N = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
mc = play.make_machine()
body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
tab = [body[0x8DBE - 0x4000 + 2 * i] | body[0x8DBE - 0x4000 + 2 * i + 1] << 8 for i in range(48)]
STOP = 0x0300
mc.m[STOP] = 0xEA
cpu = mc.cpu
rng = random.Random(30)
recs = []
for entry in tab:
    for k in range(N):
        gx, gy = rng.randrange(256), rng.randrange(256)
        a = rng.randrange(4)
        if rng.random() < 0.5:
            x, y = rng.randrange(256), rng.randrange(256)
        else:
            x = (gx + rng.choice([-3, -2, -1, 0, 0, 1, 2, 3, 40, -40])) & 255
            y = (gy + rng.choice([-3, -2, -1, 0, 0, 1, 2, 3, 40, -40])) & 255
        gdir = a if rng.random() < 0.8 else rng.randrange(4)
        pdir, ptun = rng.randrange(4), rng.randrange(2)
        mc.m[0x70], mc.m[0x71], mc.m[0x72], mc.m[0x62], mc.m[0x69] = gx, gy, gdir, pdir, ptun
        cpu.a, cpu.x, cpu.y = a, x, y
        cpu.sp = 0xF0
        cpu.pc = entry
        cpu.push((STOP - 1) >> 8)
        cpu.push((STOP - 1) & 255)
        n = 0
        while cpu.pc != STOP and n < 2000:
            cpu.step()
            n += 1
        recs.append(struct.pack('<HBBBBBBBB', entry, a, x, y, gx, gy, gdir, pdir, ptun) + bytes([cpu.a, mc.m[0x70], mc.m[0x71], mc.m[0x72], 1 if cpu.pc == STOP else 0]))
with open('scratch/decide_vectors.bin', 'wb') as f:
    f.write(b'DECV' + struct.pack('<I', len(recs)))
    for r in recs:
        f.write(r)
print('vectors', len(recs), 'reached RTS', sum(r[-1] for r in recs))
