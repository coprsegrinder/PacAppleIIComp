"""TASK-033-4b: cycle-exact model of the attract title (from the page clear after the logo to the start of the chase), written the way the C++ will be and checked against
scratch/events_title.json (development tool; original data from scratch/).  Run: python -I tools/title_model.py"""
import json, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine

mc = make_machine()
M = mc.m
D = json.load(open('scratch/events_title.json'))
EV = D['events']


class Sim:
    def __init__(self, t):
        self.t = t
        self.ev = []
        self.z = {k: 0 for k in (0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x18, 0x1A, 0x1E, 0xE0)}
        self.z[0x1D] = 1
        self.p55b2 = 0           # patched operand of LDA $0666,X in the blit
        self.p6951 = 0           # patched table pointer of the tile routine

    def add(self, n):
        self.t += n


def cross(base, idx):
    return 1 if (base & 255) + idx > 255 else 0


def branch(taken, nxt, target):
    if not taken:
        return 2
    return 3 + (1 if (nxt >> 8) != (target >> 8) else 0)


# ---- small routines ---------------------------------------------------------------------------------------------------------------------------------------------
def r_5b58(s):                         # $5B58 row address
    y = s.z[0x10]
    s.add(3 + 4 + cross(0x5B67, y) + 3 + 4 + cross(0x5C27, y) + 2 + 3 + 6)
    return M[0x5B67 + y]


def r_5933(s):                         # $5933 column/shift from x ($0C, $0D == 0)
    x = s.z[0x0C]
    s.add(3 + 3 + 3 + 4 + cross(0x5A58, x) + 3 + 4 + cross(0x5958, x) + 3 + 6)
    s.z[0x04] = M[0x5A58 + x]
    s.z[0x03] = M[0x5958 + x]


def r_poll(s):
    s.ev.append((s.t, 'poll'))
    s.add(62)


def r_delay(s, x):
    for i in range(x):
        s.ev.append((s.t, 'delay', x - i, 0))
        s.add(2 + 143 * 7 + 6 + 2)
        s.add(branch(i < x - 1, 0x6CF2, 0x6CE8))
    s.add(6)


def r_blit(s):                         # $55A6 (the loop entries are the events)
    w = s.z[0x1A]
    while True:
        data = s.p55b2 + s.z[0x03]
        s.ev.append((s.t, 'blit', data, s.z[0x04], s.z[0x10], s.z[0x1A], s.z[0x18]))
        s.add(6)
        lo12 = r_5b58(s)
        s.add(3 + 3 + 3 + 3)
        x = s.z[0x03]
        yy = s.z[0x04]
        for b in range(w):
            s.add(4 + cross(s.p55b2, x) + 5 + cross(lo12, yy) + 6 + 2 + 2 + 5)
            s.add(3 if b < w - 1 else 2)
            x += 1
            yy += 1
        s.z[0x03] = x
        s.z[0x10] += 1
        s.z[0x18] -= 1
        s.add(3 + 5 + 5)
        if s.z[0x18] != 0:
            s.add(3)
        else:
            s.add(2)
            break
    s.add(6)


def r_dot(s):                          # $6172
    s.ev.append((s.t, 'dot', s.z[0x0C], s.z[0x10]))
    s.add(6)
    r_5933(s)
    s.add(5)                           # ASL $03
    s.z[0x03] = (s.z[0x03] << 1) & 255
    for half in range(2):
        s.add(6)
        lo12 = r_5b58(s)
        x = s.z[0x03]
        yy = s.z[0x04]
        s.add(3 + 3)
        s.add(4 + cross(0x61A8, x) + 5 + cross(lo12, yy) + 6 + 2 + 2)
        s.add(4 + cross(0x61A8, x + 1) + 5 + cross(lo12, yy + 1) + 6)
        if half == 0:
            s.add(5)                   # INC $10
            s.z[0x10] += 1
    s.add(6)


def r_tile(s):                         # $693A
    s.ev.append((s.t, 'tile', s.p6951, s.z[0x0C] | s.z[0x0D] << 8, s.z[0x10]))
    s.add(2 + 3)
    s.z[0x18] = 0xF8
    s.add(6)
    r_5933(s)
    s.add(3 + 8 + 3)
    s.z[0x03] = (s.z[0x03] << 4) & 255
    while True:
        s.add(6)
        lo12 = r_5b58(s)
        x = s.z[0x03]
        yy = s.z[0x04]
        s.add(3 + 3)
        s.add(4 + cross(s.p6951, x) + 5 + cross(lo12, yy) + 6 + 2 + 2)
        s.add(4 + cross(s.p6951, x + 1) + 5 + cross(lo12, yy + 1) + 6 + 2)
        s.z[0x03] = x + 2
        s.z[0x10] += 1
        s.z[0x18] = (s.z[0x18] + 1) & 255
        s.add(3 + 5 + 5)
        if s.z[0x18] != 0:
            s.add(3)
        else:
            s.add(2)
            break
    s.add(6)


def r_7159(s, a):                      # $7159: tile from the table at $717D (the JSR is paid by the caller)
    s.add(2 + 2 + 4 + 4 + 4 + 4 + 4 + 4 + 3 + 3 + 3 + 3 + 3 + 3)
    s.p6951 = M[0x717D + 2 * a] | M[0x717E + 2 * a] << 8
    s.z[0x0C], s.z[0x0D], s.z[0x10] = s.z[0x0E], s.z[0x0F], s.z[0x11]
    s.add(6)
    r_tile(s)
    s.add(6)


def r_61b6(s):
    s.add(2 + 4 + 4 + 2 + 4 + 4)
    s.p6951 = 0x61CA
    s.add(6)
    r_tile(s)
    s.add(6)


def r_44f1(s):
    s.z[0x1A], s.z[0x18], s.p55b2, s.z[0x03] = 4, 7, 0x450A, 0
    s.add(2 + 3 + 2 + 3 + 2 + 4 + 2 + 4 + 2 + 3 + 3)
    r_blit(s)


def r_dispatch(s, a, y):               # $6A20 with $1D <> 0: the OR path for $1E <> 0, otherwise the AND $7F path ($6A28)
    s.ev.append((s.t, 'dispatch', a | y << 8, s.z[0x0C] | s.z[0x0D] << 8, s.z[0x10], s.z[0x1D], s.z[0x1E]))
    s.add(3 + 2 + 3)                   # LDX $1D, BEQ (not taken), LDX $1E
    if s.z[0x1E] != 0:
        s.add(branch(True, 0x6A28, 0x6A62))
        s.p55b2 = a | y << 8
        s.add(4 + 4)
        s.add(6 + 26)                  # JSR $7D9A: x -= 4, y -= 4
        s.z[0x0C] = (s.z[0x0C] - 4) & 255
        s.z[0x10] = (s.z[0x10] - 4) & 255
        s.add(2 + 3)
        s.z[0x18] = 10
        s.add(6)
        r_5933(s)
        s.add(6 + 3 + 4 + 3 + 6)       # JSR $780A
        s.z[0x03] = M[0x7812 + s.z[0x03]]
        s.add(2 + 3)
        s.z[0x1A] = 3
        s.add(3)                       # JMP $55A6
        r_blit(s)
        return
    s.add(2)                           # BNE not taken
    s.p55b2 = a | y << 8
    s.add(4 + 4)
    s.add(6 + 26)
    s.z[0x0C] = (s.z[0x0C] - 4) & 255
    s.z[0x10] = (s.z[0x10] - 4) & 255
    s.add(2 + 3 + 2 + 3)
    s.z[0x18], s.z[0x1A] = 10, 3
    s.add(6)
    r_5933(s)
    s.add(6 + 3 + 4 + 3 + 6)
    s.z[0x03] = M[0x7812 + s.z[0x03]]
    while True:                        # $6A3F: ten rows of three bytes, AND #$7F after the OR
        s.add(6)
        lo12 = r_5b58(s)
        s.add(3 + 3 + 3 + 3)
        x = s.z[0x03]
        yy = s.z[0x04]
        for b in range(3):
            s.add(4 + cross(s.p55b2, x) + 5 + cross(lo12, yy) + 2 + 6 + 2 + 2 + 5)
            s.add(3 if b < 2 else 2)
            x += 1
            yy += 1
        s.z[0x03] = x
        s.z[0x10] += 1
        s.z[0x18] -= 1
        s.add(3 + 5 + 5)
        if s.z[0x18] != 0:
            s.add(3)
        else:
            s.add(2)
            break
    s.add(6)


def r_88a2(s):                         # ghost image of $E0 (title: $78 = $79 = 0, $1D = 1)
    x = 2 * s.z[0xE0]
    s.add(3 + 2 + 2 + 3 + 3 + 3 + 3)   # LDA E0, ASL, TAX, LDA $78, BEQ(taken), LDA $79, BEQ(taken)
    s.add(3 + 2)                       # LDA $1D, BEQ (not taken)
    s.add(2 + 2 + 2 + 2 + 4 + 3 + 2 + 2 + 2)    # CLC, TXA, ROR, TAY, LDA $88E1,Y, STA $1E, TXA, ADC #$0C, TAX
    s.z[0x1E] = M[0x88E1 + (x >> 1)]
    x = x + 12
    a = M[0x9E5C + x]
    y = M[0x9E5D + x]
    s.add(4 + cross(0x9E5C, x) + 4 + cross(0x9E5D, x))
    s.add(6)
    r_dispatch(s, a, y)
    s.add(3 + 2 + 6)                   # LDA $81, BEQ (not taken), RTS


# ---- text -------------------------------------------------------------------------------------------------------------------------------------------------------
def text_line(s, ptr, y_idx):
    """$42D0 (the JSR itself has been paid). Leaves the new Y in $0B."""
    lo14 = ptr & 255

    def rd(i):
        s.add(5 + (1 if (lo14 + i) > 255 else 0))
        return M[ptr + i]
    s.z[0x04] = rd(y_idx)
    s.add(3)
    y_idx += 1
    s.add(2)
    s.z[0x10] = rd(y_idx)
    s.add(3 + 3)
    s.z[0x0A] = s.z[0x10]
    y_idx += 1
    s.add(2)
    s.add(3)
    s.z[0x0B] = y_idx
    while True:
        s.add(3)
        c = rd(s.z[0x0B])
        if c & 0x80:
            s.add(branch(True, 0x42E4, 0x4303))
            s.add(5 + 6)
            s.z[0x0B] += 1
            return
        s.add(2)
        if c == 0:
            s.add(2 - 2 + branch(True, 0x42E6, 0x42F6))
            draw_glyph(s, c)
        else:
            s.add(2 + 2)               # BEQ not taken, CMP #$20
            if c == 0x20:
                s.add(branch(True, 0x42EA, 0x42F9))
            else:
                s.add(2 + 2)           # BEQ not taken, CMP #$21
                if c == 0x21:
                    s.add(branch(True, 0x42EE, 0x4306))
                    s.add(2 + 4 + 2 + 4)
                    s.p55b2 = 0x4673
                    s.add(6)
                    glyph_tail(s)
                    s.add(3)           # JMP $42F9
                else:
                    s.add(2 + 2)       # BEQ not taken, CMP #3
                    if c >= 3:
                        s.add(branch(True, 0x42F2, 0x42F6))
                    else:
                        s.add(2)
                        s.add(5 + 5)
                        s.z[0x10] += 2
                    draw_glyph(s, c)
        s.add(3 + 3 + 5 + 5 + branch(True, 0x4303, 0x42DE))
        s.z[0x10] = s.z[0x0A]
        s.z[0x0B] += 1
        s.z[0x04] += 1


def draw_glyph(s, code):
    s.add(6)                           # JSR $4316
    s.add(3 + 6 + 3 + 4 + 2 + 4)
    v = code * 9
    s.add(3 if v <= 255 else 8)
    s.add(2 + 4 + 2 + 4)
    s.add(3 if (v & 255) + 0x53 <= 255 else 8)
    s.p55b2 = 0x4553 + 9 * code
    glyph_tail(s)


def glyph_tail(s):                     # $4338: draw (width 1, 9 rows), key poll, delay
    s.z[0x1A], s.z[0x18], s.z[0x03] = 1, 9, 0
    s.add(15)
    s.add(6)
    r_blit(s)
    s.add(6)
    r_poll(s)
    s.add(2 + 3)
    r_delay(s, 0x50)


def run_title(t_clear):
    s = Sim(t_clear)
    ptr = 0x434F
    s.ev.append((s.t, 'clear'))
    s.add(90575)                                       # $6F46
    s.add(2 + 3 + 2 + 3 + 3 + 2 + 3 + 2 + 3 + 2)       # $43DC-$43EE: $81, $78, $79, pointer, Y
    s.add(6)
    text_line(s, ptr, 0)                               # $43F0 (line 1)

    def ghost(row, e0):
        s.add(6 + 2 + 3 + 2 + 3 + 6)                   # JSR $44E8 (JSR 6 + LDA #$49, STA $0C, LDA #0, STA $0D, RTS = 16)
        s.z[0x0C], s.z[0x0D] = 0x49, 0
        s.add(2 + 3 + 2 + 3 + 6)                       # LDA #row, STA $10, LDA #e0, STA $E0, JSR $88A2
        s.z[0x10], s.z[0xE0] = row, e0
        r_88a2(s)
        s.add(3 + 6)                                   # LDY $0B, JSR $42D0
        text_line(s, ptr, s.z[0x0B])
        s.add(3 + 6)
        text_line(s, ptr, s.z[0x0B])
    ghost(0x22, 1)
    ghost(0x31, 3)
    ghost(0x40, 2)
    ghost(0x4F, 4)
    # $4453: the dot and the energizer lines
    s.add(2 + 3 + 2 + 3 + 6)
    s.z[0x0C], s.z[0x10] = 0x74, 0x78
    r_dot(s)
    s.add(2 + 3 + 2 + 3 + 2 + 3 + 2 + 6)               # $445E: $0E, $0F, $11, A = 1, JSR $7159
    s.z[0x0E], s.z[0x0F], s.z[0x11] = 0x7C, 0, 0x75
    r_7159(s, 1)
    s.add(2 + 3 + 2 + 3 + 2 + 6)                       # CLC, LDA $0E, ADC #6, STA $0E, LDA #0, JSR $7159
    s.z[0x0E] += 6
    r_7159(s, 0)
    s.add(2 + 3 + 2 + 3 + 6)                           # $447B: $04 = $14, $10 = $75, JSR $44F1
    s.z[0x04], s.z[0x10] = 0x14, 0x75
    r_44f1(s)
    s.add(2 + 3 + 2 + 3 + 6)                           # $4486: $0C = $73, $10 = $81, JSR $61B6
    s.z[0x0C], s.z[0x10] = 0x73, 0x81
    r_61b6(s)
    s.add(2 + 3 + 2 + 3 + 2 + 6)                       # $4491: $0E = $7C, $11 = $80, A = 5, JSR $7159
    s.z[0x0E], s.z[0x11] = 0x7C, 0x80
    r_7159(s, 5)
    s.add(2 + 3 + 2 + 3 + 2 + 6)
    s.z[0x0E] += 6
    r_7159(s, 0)
    s.add(2 + 3 + 2 + 3 + 6)                           # $44AA: $04 = $14, $10 = $80, JSR $44F1
    s.z[0x04], s.z[0x10] = 0x14, 0x80
    r_44f1(s)
    s.add(2 + 3 + 2 + 3 + 2 + 3 + 2 + 3 + 2 + 3 + 2 + 4 + 2 + 4 + 6)   # $44B5: the copyright mark
    s.z[0x04], s.z[0x10], s.z[0x03], s.z[0x1A], s.z[0x18] = 0x0C, 0x9A, 0, 5, 9
    s.p55b2 = 0x4526
    r_blit(s)
    for _ in range(3):
        s.add(3 + 6)
        text_line(s, ptr, s.z[0x0B])
    s.add(3)                                           # JMP $4689
    return s


def r_5ce7(s):                         # advance one pixel position: X := X + 1, wrapping at 7 into the next byte
    s.add(2 + 2)
    s.z[0x03] += 1
    if s.z[0x03] < 7:
        s.add(branch(True, 0x5CEC, 0x5CEF))
    else:
        s.add(2 + 2 + 2)
        s.z[0x03] = 0
        s.z[0x04] += 1
    s.add(6)


def r_hline(s, lo16):                  # $5CF0 (mode $1D = 1: two pixels per step), the JSR is paid by the caller
    s.ev.append((s.t, 'hline', s.z[0x0C], s.z[0x0E], s.z[0x10], s.z[0x1D], s.z[0x1E]))
    s.add(2 + 3 + 3)
    s.z[0x0D] = s.z[0x0F] = 0
    s.add(6)
    lo12 = r_5b58(s)
    s.add(6)
    r_5933(s)
    s.add(3 + 3)
    x0, x1 = s.z[0x0C], s.z[0x0E]
    hi = 0
    first = True
    while True:
        # $5D00
        s.add(6 + 4 + cross(0x5925, s.z[0x03]) + 5 + cross(lo12, s.z[0x04]) + 6 + 6)
        s.add(5 + cross(lo12, s.z[0x04]) + 2 + 3 + 6)
        s.add(3 + 2)                   # LDA $1D, BEQ (not taken)
        s.add(6)
        r_5ce7(s)
        s.add(6)
        r_5ce7(s)
        s.add(2 + 3 + 2 + 3)           # CLC, LDA $0C, ADC #2, STA $0C
        v = s.z[0x0C] + 2
        s.z[0x0C] = v & 255
        if v > 255:
            s.add(2 + 5)
            s.z[0x0D] += 1
        else:
            s.add(3)
        s.add(3 + 3)                   # LDA $0D, CMP $0F
        d, f = s.z[0x0D], s.z[0x0F]
        if d < f:
            s.add(3)
            continue
        s.add(2)                       # BCC not taken
        if d == f:
            s.add(3)                   # BEQ taken
            s.add(3 + 3)               # LDA $0E, CMP $0C
            if s.z[0x0E] >= s.z[0x0C]:
                s.add(3)
                continue
            s.add(2)
            break
        s.add(2)                       # BEQ not taken
        s.add(branch(True, 0x5D2A, 0x5D30))
        break
    s.add(6)


def run_logo(s, lo16_start):
    """$5260 .. (entry: A = low, Y = high of the table pointer)."""
    ptr = lo16_start
    s.add(3 + 3 + 3 + 3 + 2 + 3 + 2 + 3 + 3 + 3)
    s.z[0x1D], s.z[0x1E] = 1, 0
    while True:
        s.add(2)
        lo = ptr & 255
        def rd(i):
            s.add(5 + (1 if lo + i > 255 else 0))
            return M[ptr + i]
        a = rd(0); s.add(3); s.z[0x0C] = a
        s.add(2)
        b = rd(1); s.add(3); s.z[0x0E] = b
        s.add(2)
        c = rd(2); s.add(3); s.z[0x10] = c
        s.add(2)
        if c == 0xFD:
            s.add(branch(True, 0x5287, 0x52A7))
            s.add(4 + 3 + 3)
            return
        s.add(2 + 2)                   # BEQ not taken, CMP #$FE
        if c == 0xFE:
            s.add(branch(True, 0x528B, 0x52AD)); s.add(2 + 3 + 3); s.z[0x1E] = 0
        else:
            s.add(2 + 2)               # BEQ not taken, CMP #$FF
            if c == 0xFF:
                s.add(branch(True, 0x528F, 0x52B4)); s.add(2 + 3 + 3); s.z[0x1E] = 0x80
            else:
                s.add(2 + 6)           # BEQ not taken, JSR $5CF0
                r_hline(s, ptr)
                s.add(4 + 2 + 2)       # LDA $C000, CMP #$A0, BEQ (not taken)
        s.add(2 + 3 + 2 + 3)           # CLC, LDA $16, ADC #3, STA $16
        if lo + 3 > 255:
            s.add(2 + 5)
            ptr += 3
        else:
            s.add(branch(True, 0x52A2, 0x52A4))
            ptr += 3
        s.add(3)                       # JMP $5273


def run_start():
    """From the entry of $52BB (time 0): fill, page clear, the logo, the 11 delays; returns the Sim at the second page clear ($6F46 in the title)."""
    s = Sim(0)
    s.add(16 + 2 + 2 + 256 * 25 - 1 + 6)                # four LDA, LDA #$DD, LDY #0, the 256 passes (25 cycles, the last 24), JSR $6F46
    s.ev.append((s.t - 0, 'clear1', 0))
    s.add(90575)                                       # $6F46
    s.add(2 + 2 + 6)                                   # LDA #$7A, LDY #$4A, JSR $5260
    run_logo(s, 0x4A7A)
    s.ev.append((s.t, 'poll'))
    s.add(62)                                          # JMP $6F5E (its RTS returns to the caller of $5260)
    s.add(6 + 4 + 4 + 4 + 4)                           # JSR $6F2F: the four switches, then RTS (counted below)
    s.add(6)
    s.add(2)                                           # LDY #$0A
    for k in range(11):
        s.add(2 + 6)                                   # LDX #$FA, JSR $6CE8
        r_delay(s, 250)
        s.add(4 + 2 + 2)                               # LDA $C000, CMP #$A0, BEQ (not taken)
        s.add(2 + (3 if k < 10 else 2))                # DEY, BPL
    s.add(6)                                           # RTS of $52BB
    s.add(6)                                           # JSR $6F46 at $43D9 (the returned-to call)
    return s


def compare(mine, theirs, lo, hi):
    a = [e for e in mine if lo <= e[0] <= hi]
    b = [e for e in theirs if lo <= e[0] <= hi and e[1] in ('clear', 'blit', 'poll', 'delay', 'dispatch', 'dot', 'tile')]
    print('events mine', len(a), 'theirs', len(b))
    bad = 0
    for i, (x, y) in enumerate(zip(a, b)):
        same_t = x[0] == y[0]
        same_k = x[1] == y[1]
        pa = tuple(x[2:]) if x[1] not in ('delay',) else ()
        pb = tuple(y[2:]) if y[1] not in ('delay',) else ()
        if not (same_t and same_k and pa == pb):
            bad += 1
            if bad <= 12:
                print('first differences: #%d mine %s theirs %s' % (i, x, y))
    print('mismatching events:', bad, 'of', min(len(a), len(b)))
    return bad


if __name__ == '__main__':
    clear = [e for e in EV if e[1] == 'clear'][1][0]
    s = run_title(clear)
    end = [e for e in EV if e[1] == 'dispatch' or True]
    print('model ends at', s.t, ' (the oracle starts the chase at about 12,250,257)')
    compare(s.ev, EV, clear, 12_240_000)
