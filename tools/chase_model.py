"""TASK-033-4c: cycle-exact model of the attract chase (loops A and B and the four ghost-eaten animations), checked against scratch/events_chase.json
(development tool; original data from scratch/).  Run: python -I tools/chase_model.py"""
import json, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import title_model as T
from title_model import M, Sim, cross, branch

EV = json.load(open('scratch/events_chase.json'))['events']


class Chase(Sim):
    def __init__(self, t):
        super().__init__(t)
        import collections
        self.z = collections.defaultdict(int, self.z)
        for k in (0x59, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x66, 0x68, 0x78, 0x79, 0x80, 0x81, 0xE1, 0xE2, 0x70, 0x71, 0x16, 0x17, 0x09):
            self.z.setdefault(k, 0)
        self.rec = {g: [0] * 20 for g in (1, 2, 3, 4)}
        self.p69b8 = 0x69CD
        self.buf = [0] * 30     # the shift buffer at $7E70


# ---- the copies between the ghost records and the working area ($86FC, $8712) ------------------------------------------------------------------------------------
def copy_in(s):                          # $86FC (the JSR is paid by the caller)
    e0 = s.z[0xE0]
    s.add(3 + 4 + cross(0x870D, e0) + 2 + 20 * 16 - 1 + 6)
    for i in range(20):
        s.z[0x70 + i] = s.rec[e0][i]


def copy_out(s):                         # $8712 (LDA $0070,Y 4 + STA $84,X 4 + DEX + DEY + BPL = 15 per byte)
    e0 = s.z[0xE0]
    s.add(3 + 4 + cross(0x870D, e0) + 2 + 20 * 15 - 1 + 6)
    for i in range(20):
        s.rec[e0][i] = s.z[0x70 + i]


def r_7d80(s):                           # JSR $7D80: $0C := $60, $10 := $61, $0D := 0
    s.add(6 + 3 + 3 + 3 + 3 + 2 + 3 + 6)
    s.z[0x0C], s.z[0x10], s.z[0x0D] = s.z[0x60], s.z[0x61], 0


def r_7d8d(s):                           # JSR $7D8D: $0C := $70, $10 := $71, $0D := 0
    s.add(6 + 3 + 3 + 3 + 3 + 2 + 3 + 6)
    s.z[0x0C], s.z[0x10], s.z[0x0D] = s.z[0x70], s.z[0x71], 0


def r_7d9a(s):                           # JSR $7D9A: x -= 4, y -= 4
    s.add(6 + 26)
    s.z[0x0C] = (s.z[0x0C] - 4) & 255
    s.z[0x10] = (s.z[0x10] - 4) & 255


def r_7da9(s):                           # JSR $7DA9: x -= 5, y -= 4
    s.add(6 + 26)
    s.z[0x0C] = (s.z[0x0C] - 5) & 255
    s.z[0x10] = (s.z[0x10] - 4) & 255


def r_mclear_body(s):                    # $69A1
    s.ev.append((s.t, 'mclear', s.p69b8, s.z[0x0C] | s.z[0x0D] << 8, s.z[0x10]))
    s.add(2 + 3)
    s.z[0x18] = 0xF6
    s.add(6)
    T.r_5933(s)
    s.add(5 + 5)
    s.z[0x03] = (s.z[0x03] << 2) & 255
    while True:
        s.add(6)
        lo12 = T.r_5b58(s)
        s.add(2 + 3 + 3 + 3)
        x = s.z[0x03]
        yy = s.z[0x04]
        for b in range(3):
            s.add(4 + cross(s.p69b8, x) + 5 + cross(lo12, yy) + 2 + 6 + 2 + 2 + 5)
            s.add(3 if b < 2 else 2)
            x += 1
            yy += 1
        s.z[0x10] += 1
        s.z[0x18] = (s.z[0x18] + 1) & 255
        s.add(5 + 5)
        if s.z[0x18] != 0:
            s.add(3)
        else:
            s.add(2)
            break
    s.add(6)


def r_699e(s):                           # JSR $699E: erase the 3 x 10 block at ($0C - 4, $10 - 4)
    s.add(6)
    r_7d9a_body(s)
    r_mclear_body(s)


def r_7d9a_body(s):                      # $7D9A reached by the JSR inside $699E
    s.add(6 + 26)
    s.z[0x0C] = (s.z[0x0C] - 4) & 255
    s.z[0x10] = (s.z[0x10] - 4) & 255


def r_69e9(s):                           # JSR $69E9: erase with the second mask table at ($0C - 5, $10 - 4)
    s.add(6)
    r_7da9(s)
    s.add(2 + 2 + 4 + 4)
    s.p69b8 = 0x6A04
    s.add(6)
    r_mclear_body(s)
    s.add(2 + 2 + 4 + 4)
    s.p69b8 = 0x69CD
    s.add(6)


def r_79a4(s):                           # JSR $79A4: Pac-Man's image
    s.add(6)
    a = s.z[0x66] & 0x0F
    s.add(3 + 2 + 3)
    s.z[0x66] = a
    t = a & 3
    s.add(2)
    if t == 0:
        s.add(branch(True, 0x79AE, 0x79BE))
        idx = 0
    else:
        s.add(2)
        s.add(2)
        if t == 3:
            s.add(2 + 2)                       # BNE not taken, LDA #1
            a = 1
            carry = 1
        else:
            s.add(branch(True, 0x79B2, 0x79B4))
            a = t
            carry = 0
        s.add(2)                               # ROR A
        newc = a & 1
        a = (carry << 7) | (a >> 1)
        carry = newc
        s.add(3)                               # LDA $62
        if carry:
            s.add(branch(True, 0x79B9, 0x79BC))
            idx = s.z[0x62] + 1
            s.add(2)
        else:
            s.add(2)
            s.add(2 + 2)                       # ADC #4, SEC
            idx = s.z[0x62] + 5
            s.add(2)
    s.add(2 + 2 + 2 + 3)                       # ASL, TAX, LDA #$80, STA $1E
    s.z[0x1E] = 0x80
    x = 2 * idx
    a_lo = M[0x96E8 + x]
    a_hi = M[0x96E9 + x]
    s.add(4 + cross(0x96E8, x) + 4 + cross(0x96E9, x))
    s.add(3)
    T.r_dispatch(s, a_lo, a_hi)


def r_88a2(s):                           # JSR $88A2: the ghost image (E0, $78, $79, $E1/$E2)
    s.add(6)
    x = 2 * s.z[0xE0]
    s.add(3 + 2 + 2 + 3)
    if s.z[0x78] == 0:
        s.add(branch(True, 0x88AA, 0x88BC))
    else:
        s.add(2)
        s.add(3)
        if s.z[0xE2] != 0:
            s.add(branch(True, 0x88AE, 0x88BA))
            x = 0
            s.add(2)
        else:
            s.add(2)
            s.add(3 + 2)
            if s.z[0xE1] >= 0x80:
                s.add(branch(True, 0x88B4, 0x88BA))
                x = 0
                s.add(2)
            else:
                s.add(2)
                x = 6
                s.add(2 + 2)
                if (s.z[0xE1] & 0x10) == 0:
                    s.add(branch(True, 0x88BA, 0x88C2))
                else:
                    s.add(2)
                    x = 0
                    s.add(2)
    s.add(3)
    if s.z[0x79] == 0:
        s.add(branch(True, 0x88C0, 0x88C2))
    else:
        s.add(2)
        x = 0x0A
        s.add(2)
    s.add(3 + 2)                              # LDA $1D, BEQ (not taken)
    s.add(2 + 2 + 2 + 2)
    y = x >> 1
    s.add(4 + cross(0x88E1, y) + 3)
    s.z[0x1E] = M[0x88E1 + y]
    s.add(2 + 2 + 2)
    x += 12
    a_lo = M[0x9E5C + x]
    a_hi = M[0x9E5D + x]
    s.add(4 + cross(0x9E5C, x) + 4 + cross(0x9E5D, x))
    s.add(6)
    T.r_dispatch(s, a_lo, a_hi)
    s.add(3 + 2 + 6)


def r_7819(s):                           # JSR $7819: an empty tile at ($0C, $10)
    s.add(2 + 3)                              # (the JSR is paid by the caller)
    s.z[0x18] = 0xF8
    s.add(6)
    T.r_5933(s)
    s.add(5)
    s.z[0x03] = (s.z[0x03] << 1) & 255
    while True:
        s.add(6)
        lo12 = T.r_5b58(s)
        x = s.z[0x03]
        yy = s.z[0x04]
        s.add(3 + 3)
        s.add(4 + cross(0x7843, x) + 5 + cross(lo12, yy) + 2 + 6 + 2)
        s.add(4 + cross(0x7844, x) + 5 + cross(lo12, yy + 1) + 2 + 6)
        s.z[0x10] += 1
        s.z[0x18] = (s.z[0x18] + 1) & 255
        s.add(5 + 5)
        if s.z[0x18] != 0:
            s.add(3)
        else:
            s.add(2)
            break
    s.add(6)


def r_7e4f(s):                           # $7E4F: shift the 30-byte image buffer by one pixel (10 passes of 63 cycles)
    s.add(2 + 10 * 63 + 9 * 3 + 2 + 6)


def r_7e1f(s):                           # JSR $7E1F: a shifted image (the score of an eaten ghost) at ($0C, $10)
    ptr = s.z[0x16] | s.z[0x17] << 8
    s.ev.append((s.t + 6, 'shifted', ptr, s.z[0x0C] | s.z[0x0D] << 8, s.z[0x10], s.z[0x1D], s.z[0x1E]))
    s.add(6)
    r_7da9(s)
    s.add(6)
    T.r_5933(s)
    s.add(2)
    lo16 = ptr & 255
    for i in range(29, -1, -1):
        s.add(5 + (1 if lo16 + i > 255 else 0) + 5 + 2 + (3 if i > 0 else 2))
    s.add(3)
    if s.z[0x03] == 0:
        s.add(branch(True, 0x7E33, 0x7E3C))
    else:
        s.add(2)
        s.add(3)
        for k in range(s.z[0x03]):
            s.add(6)
            r_7e4f(s)
            s.add(5)
            s.add(3 if k < s.z[0x03] - 1 else 2)
    s.add(6 + 3 + 4 + 3 + 6)                  # JSR $780A
    s.z[0x03] = M[0x7812 + s.z[0x03]]
    s.add(6 + 26)                             # JSR $804E: x += 4, y += 4
    s.z[0x0C] = (s.z[0x0C] + 4) & 255
    s.z[0x10] = (s.z[0x10] + 4) & 255
    s.add(2 + 2 + 2 + 3)
    lo = 0x70 - s.z[0x03]
    if lo >= 0:
        s.add(branch(True, 0x7E49, 0x7E4C))
        hi = 0x7E
    else:
        s.add(2 + 2)
        lo += 256
        hi = 0x7D
    s.add(3)
    T.r_dispatch(s, lo & 255, hi)


def r_467c(s):                           # JSR $467C: the ghost-eaten animation (attract mode: no sound, a delay instead)
    s.add(6)
    s.add(6)                              # JSR $7DB8
    a = s.z[0x68]
    s.add(3 + 2)
    if a == 2:
        s.add(branch(True, 0x7DBE, 0x7DCC)); ptr = 0x7FC7
        s.add(2 + 2 + 3)
    else:
        s.add(2 + 2)
        if a == 4:
            s.add(branch(True, 0x7DC2, 0x7DD2)); ptr = 0x7FE5
            s.add(2 + 2 + 3)
        else:
            s.add(2 + 2)
            if a == 8:
                s.add(branch(True, 0x7DC6, 0x7DD8)); ptr = 0x8003
                s.add(2 + 2)                  # LDA #3, LDY #$80 and on into $7DDC (no branch here)
            else:
                s.add(2)
                ptr = 0x8021
                s.add(2 + 2 + 3)
    s.add(3 + 3)
    s.z[0x16], s.z[0x17] = ptr & 255, ptr >> 8
    r_7d80(s)
    r_699e(s)
    r_7d8d(s)
    r_699e(s)
    r_7d8d(s)
    r_7e1f(s)
    s.add(3)                              # LDA $E8
    s.add(branch(True, 0x7DF6, 0x7E0E))
    s.add(2 + 6)
    T.r_delay(s, 255)
    r_7d8d(s)
    r_69e9(s)
    r_7d80(s)
    s.add(3)                              # JMP $79A4 (no JSR: the dispatch returns to $467F)
    s.add(-6)
    r_79a4(s)
    s.add(2 + 2 + 3 + 3 + 3 + 2 + 6)      # CLC, SED, LDA, ADC, STA, CLD, RTS
    v = s.z[0x68]
    # BCD addition
    lo = (v & 15) * 2
    hi = (v >> 4) * 2
    if lo > 9:
        lo -= 10
        hi += 1
    s.z[0x68] = ((hi % 10) << 4) | lo


def run_chase(t0):
    s = Chase(t0)
    z = s.z
    # $4689: start-up values (already paid: 63 cycles up to and including the JSR $8712 of the first ghost)
    z[0x5E], z[0x59], z[0x80], z[0x62], z[0x66], z[0x78], z[0x79], z[0x81], z[0x5F], z[0x60], z[0x71], z[0x61], z[0xE0] = 0x70, 0x20, 0x18, 3, 0, 0, 0, 1, 1, 0x8D, 0x68, 0x68, 4
    z[0x70] = 0
    # the working area before the first copy holds the same values for all four records: x unknown but overwritten below, y = $68, flags 0, counter $18
    base = [0] * 20
    base[1], base[8], base[9], base[16], base[17] = 0x68, 0, 0, 0x18, 1
    for g in (1, 2, 3, 4):
        s.rec[g] = list(base)
    # t0 is the cycle at which JSR $8712 starts its work (the JSR is paid)
    for e0 in (4, 3, 2, 1):
        z[0xE0] = e0
        copy_out(s)
        s.add(5)                          # DEC $E0
        s.add(3 if e0 > 1 else 2)         # BNE $46B5 -> JSR is 6 more at the next pass
        if e0 > 1:
            s.add(6)
    z[0xE0] = 0
    s.add(2 + 3 + 2 + 3 + 2 + 3 + 2 + 3)  # $84, $98, $AC, $C0
    s.rec[1][0], s.rec[2][0], s.rec[3][0], s.rec[4][0] = 0xB1, 0xC1, 0xD1, 0xE1
    # ---- loop A ($46CC)
    while True:
        s.add(5)                          # DEC $59
        z[0x59] -= 1
        if z[0x59] != 0:
            s.add(branch(True, 0x46D0, 0x46EC))
        else:
            s.add(2)
            s.add(6)
            T.r_poll(s)
            s.add(2 + 3)
            z[0x59] = 0x20
            r_7d80(s)
            r_699e(s)
            s.add(2 + 3 + 2 + 3)          # SEC LDA SBC STA
            z[0x60] -= 2
            r_7d80(s)
            s.add(5)
            z[0x66] += 1
            r_79a4(s)
        s.add(6 + 6)                      # JSR $6CF3 (RTS)
        s.add(2 + 3)
        z[0xE0] = 4
        while True:
            s.add(6)
            copy_in(s)
            s.add(5)
            z[0x80] -= 1
            if z[0x80] != 0:
                s.add(branch(True, 0x46FA, 0x4711))
            else:
                s.add(2)
                s.add(2 + 3)
                z[0x80] = 0x18
                r_7d8d(s)
                r_699e(s)
                s.add(2 + 3 + 2 + 3)
                z[0x70] -= 2
                r_7d8d(s)
                r_88a2(s)
            s.add(6)
            copy_out(s)
            s.add(5)
            z[0xE0] -= 1
            if z[0xE0] != 0:
                s.add(branch(True, 0x4716, 0x46F3))
            else:
                s.add(2)
                break
        s.add(5)                          # DEC $5E
        z[0x5E] -= 1
        if z[0x5E] != 0:
            s.add(branch(True, 0x471C, 0x4742))
        else:
            s.add(2)
            s.add(2 + 3)
            z[0x5E] = 0x60
            s.add(3)                      # LDA $5F
            if z[0x5F] != 0:
                s.add(2)
                s.add(2 + 3 + 2 + 3 + 6)
                z[0x10], z[0x0C] = 0x67, 0x4B
                T.r_61b6(s)
                s.add(2)
                s.add(branch(True, 0x4733, 0x4740))   # LDA #0 / BEQ $4740
                z[0x5F] = 0
                s.add(3)
            else:
                s.add(branch(True, 0x4724, 0x4733))
                s.add(2 + 3 + 2 + 3 + 6)
                z[0x10], z[0x0C] = 0x66, 0x4A
                r_7819(s)
                s.add(2 + 3)
                z[0x5F] = 1
        s.add(3 + 2)                      # LDA $60, CMP #$4F
        if z[0x60] < 0x4F:
            s.add(branch(True, 0x4748, 0x474B))
            break
        s.add(2)
        s.add(3)                          # JMP $46CC
    # ---- loop B ($474B)
    s.add(2 + 3 + 3 + 3 + 3 + 2 + 3 + 2 + 3 + 3)
    for g in (1, 2, 3, 4):
        s.rec[g][8] = 1
    z[0xE2] = 0xFF
    z[0x68] = 2
    z[0x62] = 2
    while True:
        s.add(5)                          # DEC $59
        z[0x59] -= 1
        if z[0x59] != 0:
            s.add(branch(True, 0x4763, 0x477F))
        else:
            s.add(2)
            s.add(6)
            T.r_poll(s)
            s.add(2 + 3)
            z[0x59] = 0x20
            r_7d80(s)
            r_699e(s)
            s.add(2 + 3 + 2 + 3)          # CLC LDA ADC STA
            z[0x60] += 2
            r_7d80(s)
            s.add(5)
            z[0x66] += 1
            r_79a4(s)
        s.add(6 + 6)
        s.add(2 + 3)
        z[0xE0] = 4
        done = False
        while True:
            s.add(6)
            copy_in(s)
            s.add(5)
            z[0x80] -= 1
            if z[0x80] != 0:
                s.add(branch(True, 0x478D, 0x47C0))
            else:
                s.add(2)
                s.add(2 + 3)
                z[0x80] = 0x40
                s.add(3)                  # LDA $79
                if z[0x79] != 0:
                    s.add(branch(True, 0x4793, 0x47C0))
                else:
                    s.add(2)
                    s.add(2 + 3 + 3 + 2)  # SEC LDA SBC CMP
                    diff = (z[0x70] - z[0x60]) & 255
                    if diff >= 5:
                        s.add(branch(True, 0x479E, 0x47AD))
                        r_7d8d(s)
                        r_699e(s)
                        s.add(2 + 3 + 2 + 3)
                        z[0x70] += 2
                        r_7d8d(s)
                        r_88a2(s)
                    else:
                        s.add(2)
                        r_467c(s)
                        s.add(3 + 2)      # LDA $68, CMP #$32
                        if z[0x68] == 0x32:
                            s.add(branch(True, 0x47A7, 0x47C9))
                            done = True
                            break
                        s.add(2)
                        s.add(2 + 3)
                        z[0x79] = 1
                        s.add(branch(True, 0x47AD, 0x47C0))
            s.add(6)
            copy_out(s)
            s.add(5)
            z[0xE0] -= 1
            if z[0xE0] != 0:
                s.add(branch(True, 0x47C5, 0x4786))
            else:
                s.add(2)
                s.add(branch(True, 0x47C9, 0x475F))
                break
        if done:
            break
    s.add(2 + 2 + 3 + 3 + 2 + 3 + 3 + 3)       # $47C9-$47D7: up to and including the JMP $5303
    return s


def compare(mine, theirs, lo, hi):
    a = [e for e in mine if lo <= e[0] <= hi]
    b = [e for e in theirs if lo <= e[0] <= hi and e[1] in ('blit', 'poll', 'delay', 'dispatch', 'tile', 'mclear', 'shifted')]
    print('events mine', len(a), 'theirs', len(b))
    bad = 0
    for i, (x, y) in enumerate(zip(a, b)):
        pa = tuple(x[2:]) if x[1] != 'delay' else ()
        pb = tuple(y[2:]) if y[1] != 'delay' else ()
        if not (x[0] == y[0] and x[1] == y[1] and pa == pb):
            bad += 1
            if bad <= 10:
                print('#%d mine %s | theirs %s' % (i, x, y))
    print('mismatching:', bad, 'of', min(len(a), len(b)))
    return bad


if __name__ == '__main__':
    # t0 = cycle at which the first JSR $8712 of the chase starts: the title script ends at 12,250,257 with the JSR just paid
    s = run_chase(12250257)
    print('chase model ends at', s.t)
    compare(s.ev, EV, 12250257, 22519000)
