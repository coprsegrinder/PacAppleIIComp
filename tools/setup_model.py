"""TASK-033-4d: cycle-exact model of the demo game's set-up (JSR $531E ... the first tick at $530F), checked against scratch/events_demosetup.json
(development tool; original data from scratch/).  Run: python -I tools/setup_model.py"""
import json, sys, os, collections
sys.path.insert(0, os.path.dirname(__file__))
import title_model as T
import chase_model as C
from title_model import M, cross, branch

EV = json.load(open('scratch/events_demosetup.json'))['events']


class S(C.Chase):
    def __init__(self, t):
        super().__init__(t)
        self.mem = collections.defaultdict(int)       # the few bytes of RAM the set-up reads back (map, player data)
        self.mapp = list(M[0x65BA:0x65BA + 896])      # the pristine maze
        self.z[0x50] = self.z[0x51] = self.z[0x55] = 1
        self.z[0x58] = 0
        self.z[0xE8] = 1
        self.mem_5592, self.mem_5597 = 0xF7, 0x55


def hex_lines(s):
    pass


# ---- $531E: copy the maze, save the player data ------------------------------------------------------------------------------------------------------------------
def r_5331(s):
    s.add(6)
    s.add(5 * 4 + 2)
    p14, p16 = 0x65BA, 0x623A
    while True:
        s.add(5 + 6)                        # LDA ($14),Y, STA ($16),Y
        s.add(5)                            # INC $16
        p16 += 1
        if (p16 & 255) != 0:
            s.add(3)
        else:
            s.add(2 + 5)
        s.add(5)                            # INC $14
        p14 += 1
        if (p14 & 255) != 0:
            s.add(3)
        else:
            s.add(2 + 5)
        s.add(3 + 2)                        # LDA $17, CMP #$65
        if (p16 >> 8) != 0x65:
            s.add(3)
            continue
        s.add(2)
        s.add(3 + 2)                        # LDA $16, CMP #$BA
        if (p16 & 255) != 0xBA:
            s.add(3)
            continue
        s.add(2)
        break
    s.add(6)


def r_53be(s):                              # save the player's data ($52-$59 -> $0300+)
    s.add(6)
    s.add(3 + 4 + cross(0x53E0, s.z[0x50]) + 2 + 8 * 16 - 1 + 6)


def r_531e(s):                              # JSR $531E
    s.add(6 + 2 + 3)
    r_5331(s)
    s.add(2 + 3 + 2 + 3)
    r_53be(s)
    s.add(6)


def r_vline(s):                             # $5D3D (JSR paid by the caller): one pixel per row from $10 to $11
    s.ev.append((s.t, 'vline', s.z[0x0C], s.z[0x10], s.z[0x11]))
    s.add(6)
    T.r_5933(s)
    while True:
        s.add(6)
        T.r_5b58(s)
        s.add(3 + 3)
        s.add(6 + 4 + cross(0x5925, s.z[0x03]) + 5 + cross(M[0x5B67 + s.z[0x10]], s.z[0x04]) + 6 + 6)
        s.add(5)
        s.z[0x10] += 1
        s.add(3 + 3)
        if s.z[0x11] >= s.z[0x10]:
            s.add(3)
            continue
        s.add(2)
        break
    s.add(6)


def r_5d53(s):                              # JSR $5D53: the maze walls from the table at $5F69
    s.add(6)
    s.add(2 + 3 + 3 + 2 + 2 + 3 + 2 + 3 + 3)
    s.z[0x1E] = 0x80
    ptr = 0x5F69
    s.z[0x0D] = 0
    s.add(2 + 3 + 2 + 3)                    # $5D6E: LDA #0, STA $0D, LDA #$48, STA $08
    for k in range(72):
        s.add(2)
        lo = ptr & 255
        s.z[0x0C] = M[ptr]; s.add(5 + cross(lo, 0) + 3 + 2)
        s.z[0x0E] = M[ptr + 1]; s.add(5 + cross(lo, 1) + 3 + 2)
        s.z[0x10] = M[ptr + 2]; s.add(5 + cross(lo, 2) + 3)
        s.add(6)
        T.r_hline(s, ptr)
        s.add(2 + 2 + 3 + 3)
        if lo + 3 > 255:
            s.add(2 + 5)
        else:
            s.add(3)
        ptr += 3
        s.add(5)
        s.add(3 if k < 71 else 2)
    s.add(2 + 3)
    for k in range(70):
        s.add(2)
        lo = ptr & 255
        s.z[0x0C] = M[ptr]; s.add(5 + cross(lo, 0) + 3 + 2)
        s.z[0x10] = M[ptr + 1]; s.add(5 + cross(lo, 1) + 3 + 2)
        s.z[0x11] = M[ptr + 2]; s.add(5 + cross(lo, 2) + 3)
        s.add(6)
        r_vline(s)
        s.add(2 + 3 + 2 + 3)
        if lo + 3 > 255:
            s.add(2 + 5)
        else:
            s.add(3)
        ptr += 3
        s.add(5)
        s.add(3 if k < 69 else 2)
    s.add(6)


def r_8a7d(s):                              # JSR $8A7D: the two lines of the ghost-house door
    s.add(6)
    for y in (0x4D, 0x4E):
        s.add(15)
        s.z[0x0C], s.z[0x0E], s.z[0x10] = 0x87, 0x93, y
        s.add(6)
        T.r_hline(s, 0)
    s.add(6)


def r_53cf(s):                              # JSR $53CF: load the player's data
    s.add(6 + 3 + 4 + cross(0x53E0, s.z[0x50]) + 2 + 8 * 15 - 1 + 6)            # (LDA $0300,Y 4 + STA $52,X 4 + DEY + DEX + BPL = 15 a byte)


def r_6113(s):                              # JSR $6113: the dots and energizers of the map
    s.add(6)
    s.add(15 + 10)
    p16 = 0x623A
    z0e = 0x3B
    col = 0
    while True:
        y11 = 0x0B
        for row in range(1, 31):
            s.add(3)
            cell = s.mapp[col * 32 + row]
            lo16 = p16 & 255
            s.add(5 + cross(lo16, row))
            if cell == 0:
                s.add(3)
            else:
                s.add(2)
                s.add(2)                    # ROR A
                if cell & 1:
                    s.add(3)
                    s.add(3 + 3 + 3 + 3)
                    s.z[0x0C], s.z[0x10] = z0e, y11
                    s.add(6)
                    T.r_dot(s)
                else:
                    s.add(2)
                    s.add(3 + 3 + 5 + 3 + 3 + 5)
                    s.z[0x0C], s.z[0x10] = z0e - 1, y11 - 1
                    s.add(6)
                    s.z[0x0D] = 0
                    T.r_61b6(s)
                    s.add(2 + 3)
            s.add(2 + 3 + 2 + 3)
            y11 += 6
            s.add(5 + 3 + 2)
            if row + 1 < 0x1F:
                s.add(3)
            else:
                s.add(2)
        s.add(3 + 2 + 3)
        v = (p16 & 255) + 0x1F + 1
        p16 = (p16 & 0xFF00) | (v & 255)
        if v > 255:
            s.add(2 + 5)
            p16 += 0x100
        else:
            s.add(3)
        s.add(2 + 3 + 2 + 3 + 2)
        z0e += 6
        col += 1
        if z0e < 0xDD:
            s.add(3)
            s.add(10)
            continue
        s.add(2)
        break
    s.add(6)


def r_5659_head(s):
    s.add(6)                                # JSR $5659
    s.ev.append((s.t + 6, 'clear'))
    s.add(6 + 90575)                        # JSR $6F46
    s.add(4)                                # STA $C010
    s.add(6 + 4 + 4 + 4 + 4 + 6)            # JSR $6F2F
    r_5d53(s)
    r_8a7d(s)
    s.add(3 + 3)                            # LDA $50, STA $F1
    r_53cf(s)
    r_6113(s)
    r_550a(s)
    s.add(3 + 3)                            # LDA $F1, STA $50
    s.z[0x50] = 1
    r_53cf(s)
    r_6f13(s)
    s.add(6 + 3 + 2 + 3 + 3 + 3 + 6)       # JSR $56CF (the demo has no life icons: LDA $58, AND, STA, LDA $E8, BNE, RTS)
    s.add(3 + 3 + 6)                        # LDA $E8, BNE $5691, RTS of $5659


def r_5303(s):
    r_531e(s)
    r_5659_head(s)
    r_5850(s)
    r_53e6(s)


def r_hline_mid(s):                         # JSR $5CF6: the dotted line, entered after "LDA #0 / STA $0D / STA $0F" ($0D and $0F are set by the caller)
    s.add(6 + 6)                            # JSR $5CF6, JSR $5B58
    lo12 = T.r_5b58(s)
    s.add(6)
    T.r_5933(s)
    s.add(3 + 3)
    while True:
        s.add(6 + 4 + cross(0x5925, s.z[0x03]) + 5 + cross(lo12, s.z[0x04]) + 6 + 6)
        s.add(5 + cross(lo12, s.z[0x04]) + 2 + 3 + 6)
        s.add(3 + 2)
        s.add(6)
        T.r_5ce7(s)
        s.add(6)
        T.r_5ce7(s)
        s.add(2 + 3 + 2 + 3)
        v = s.z[0x0C] + 2
        s.z[0x0C] = v & 255
        if v > 255:
            s.add(2 + 5)
            s.z[0x0D] += 1
        else:
            s.add(3)
        s.add(3 + 3)
        d, f = s.z[0x0D], s.z[0x0F]
        if d < f:
            s.add(3)
            continue
        s.add(2)
        if d == f:
            s.add(3)
            s.add(3 + 3)
            if s.z[0x0E] >= s.z[0x0C]:
                s.add(3)
                continue
            s.add(2)
            break
        s.add(2)
        s.add(branch(True, 0x5D2A, 0x5D30))
        break
    s.add(6)


def r_696a(s):                              # JSR $696A: clear the score box of player $50
    s.ev.append((s.t + 6, 'box', s.z[0x50]))
    s.add(6)
    x = 2 * s.z[0x50]
    s.add(3 + 2 + 2 + 4 + cross(0x6992, x) + 3 + 4 + cross(0x6993, x) + 3)
    s.z[0x04] = M[0x6992 + x]
    s.z[0x10] = M[0x6993 + x]
    s.add(2 + 3)
    s.z[0x18] = 12
    while True:
        s.add(6)
        lo12 = T.r_5b58(s)
        s.add(3 + 2 + 2)
        yy = s.z[0x04]
        for b in range(8):
            s.add(6 + 2 + 2 + (3 if b < 7 else 2) + (1 if lo12 + yy > 255 else 0))
            yy += 1
        s.add(5 + 5)
        s.z[0x10] += 1
        s.z[0x18] -= 1
        if s.z[0x18] != 0:
            s.add(3)
        else:
            s.add(2)
            break
    s.add(6)


def r_70f5(s):                              # $70F5: the digits of the score ($05-$07), right to left
    while True:
        s.add(2 + 2)
        a = 0
        for _ in range(4):
            s.add(2 + 5 + 5 + 5 + 2 + 2)
            c = s.z[0x07] & 1
            s.z[0x07] >>= 1
            c2 = s.z[0x06] & 1
            s.z[0x06] = (c << 7) | (s.z[0x06] >> 1)
            c3 = s.z[0x05] & 1
            s.z[0x05] = (c2 << 7) | (s.z[0x05] >> 1)
            a = (c3 << 7) | (a >> 1)
        s.add(3 * branch(True, 0x7104, 0x70F9) + 2)   # BNE taken x3 (it crosses a page: 4 cycles), not taken
        s.add(2 + 2 + 2 + 2 + 2)
        a = a >> 4
        s.add(6)
        C_7159(s, a)
        s.add(2 + 3 + 2 + 3)
        v = s.z[0x0E] - 6
        s.z[0x0E] = v & 255
        if v < 0:
            s.add(2 + 5)
            s.z[0x0F] -= 1
        else:
            s.add(3)
        s.add(3)
        if s.z[0x07] != 0:
            s.add(branch(True, 0x711B, 0x70F5))
            continue
        s.add(2 + 3)
        if s.z[0x06] != 0:
            s.add(branch(True, 0x711F, 0x70F5))
            continue
        s.add(2 + 3)
        if s.z[0x05] != 0:
            s.add(branch(True, 0x7123, 0x70F5))
            continue
        s.add(2)
        break
    s.add(6)


def C_7159(s, a):
    T.r_7159(s, a)


def r_70cc(s):                              # JSR $70CC: erase the score box and draw the score digits
    s.add(6)
    s.add(3 + 3 + 3 + 3 + 3 + 3)
    s.z[0x05], s.z[0x06], s.z[0x07] = s.z[0x52], s.z[0x53], s.z[0x54]
    r_696a(s)
    x = 4 * s.z[0x50]
    s.add(3 + 2 + 2 + 2 + (4 + cross(0x7141, x) + 3) + (4 + cross(0x7142, x) + 3) + (4 + cross(0x7143, x) + 3))
    s.z[0x0E] = M[0x7141 + x]
    s.z[0x0F] = M[0x7142 + x]
    s.z[0x11] = M[0x7143 + x]
    s.add(6)
    r_70f5(s)
    s.add(3)                                    # JMP $7124
    s.add(3)
    if s.z[0x50] == 0:
        s.add(3)                                # BEQ $7140
    else:
        s.add(2 + 3)                            # BEQ not taken, BIT $58
        assert (s.z[0x58] & 0x80) == 0, 'bonus life check not needed in the demo'
        s.add(3)                                # BPL $7140
    s.add(6)


def r_556e(s, x):                           # JSR $556E: the label of the score
    s.add(6)
    for k in range(5):
        s.add(4 + cross(0x55C7 + k, x) + 3)
    s.z[0x04] = M[0x55C7 + x]; s.z[0x10] = M[0x55C8 + x]; s.z[0x0E] = M[0x55C9 + x]; s.z[0x0F] = M[0x55CA + x]; s.z[0x11] = M[0x55CB + x]
    s.add(2 + 3 + 2 + 3 + 3 + 2 + 4 + 2 + 4)
    s.z[0x03], s.z[0x18], s.z[0x1A] = 0, 7, 7
    s.p55b2 = s.mem_5592 | s.mem_5597 << 8
    s.add(6)
    T.r_blit(s)
    s.add(3)
    if s.z[0x50] == 0:
        s.add(3)
    else:
        s.add(2)
        s.add(6)
        T.r_7159(s, s.z[0x50])
    s.add(6)


def r_550a(s):                              # JSR $550A: the labels, scores and the high score
    s.add(6)
    s.mem_5592, s.mem_5597 = 0xF7, 0x55
    s.add(3 + 3)
    s.z[0x50] = s.z[0x51]
    while True:
        s.add(3 + 2 + 2 + 2 + 2)
        x = 8 * s.z[0x50]
        s.add(4 + cross(0x55CC, x) + 3 + 4 + cross(0x55CD, x) + 3)
        s.z[0x0B] = M[0x55CC + x]
        s.z[0x0A] = M[0x55CD + x]
        r_556e(s, x)
        s.add(2 + 3 + 3)
        s.z[0x0D] = s.z[0x0F] = 0
        s.add(3 + 3 + 2 + 2 + 3)
        s.z[0x0C] = s.z[0x0B]
        v = s.z[0x0B] + 0x31
        s.z[0x0E] = v & 255
        if v > 255:
            s.add(2 + 5)
            s.z[0x0F] += 1
        else:
            s.add(3)
        r_hline_mid(s)
        s.add(3)
        if s.z[0x50] == 0:
            s.add(3)
        else:
            s.add(2)
            r_53cf(s)
        r_70cc(s)
        s.add(5)
        s.z[0x50] = (s.z[0x50] - 1) & 255
        if s.z[0x50] & 0x80:
            s.add(3)
            break
        s.add(2)
        if s.z[0x50] == 0:
            s.add(3)
            s.add(2 + 4 + 2 + 4 + 3 + 3 + 3 + 3 + 3 + 3 + 3)      # $554A: patch the label pointer, copy the score of the high score
            s.mem_5592, s.mem_5597 = 0x28, 0x56
            s.z[0x52], s.z[0x53], s.z[0x54] = s.z[0x00], s.z[0x01], s.z[0x02]
        else:
            s.add(2)
            s.add(3)
    s.add(2 + 4 + 2 + 4 + 6)
    s.mem_5592, s.mem_5597 = 0xF7, 0x55


def r_6f13(s):                              # JSR $5692 -> JMP $6F13: the level's fruit as a 12 x 3 image
    s.add(6 + 3)
    s.add(3 + 2 + 3)
    lvl = min(s.z[0x55], 0x1A)
    s.add(2 + 2 + 2 + 2 + 3 + 2 + 3)
    x = 4 * lvl
    s.add(4 + cross(0x6AFE, x) + 4 + cross(0x6AFF, x) + 3)
    a, y = M[0x6AFE + x], M[0x6AFF + x]
    s.z[0x10], s.z[0x04] = 0xB0, 0x21
    s.ev.append((s.t, 'img12x3', a | y << 8, s.z[0x04], s.z[0x10]))
    s.p6f00 = a | y << 8
    s.add(4 + 4 + 2 + 3 + 2 + 3)
    s.z[0x18], s.z[0x03] = 12, 0
    while True:
        s.add(6)
        lo12 = T.r_5b58(s)
        s.add(3 + 3 + 2 + 3)
        x = s.z[0x03]
        yy = s.z[0x04]
        for b in range(3):
            s.add(5 + cross(lo12, yy) + 2 + 4 + cross(s.p6f00, x) + 6 + 2 + 2 + 5)
            s.add(branch(b < 2, 0x6F0A, 0x6EFB))
            x += 1
            yy += 1
        s.z[0x03] = x
        s.add(3 + 5 + 5)
        s.z[0x10] += 1
        s.z[0x18] -= 1
        if s.z[0x18] != 0:
            s.add(branch(True, 0x6F12, 0x6EF0))
        else:
            s.add(2)
            break
    s.add(6)


def r_5839(s):                              # JSR $5839: Pac-Man's start image (the AND path of the dispatcher, $1E = 0)
    s.add(6 + 5 + 5 + 5 + 5 + 4 + 3)
    s.z[0x0C], s.z[0x10], s.z[0x0D], s.z[0x1E] = 0x8B, 0x8F, 0, 0
    T.r_dispatch(s, 0xFA, 0x96)


def r_80e3(s):                              # JMP $80E3 (demo): the words GAME OVER
    s.add(3)
    for col_y, data in ((0x0F, 0x811D), (0x15, 0x8139)):
        s.add(2 + 2 + 2 + 6 + 3 + 3 + 3 + 2 + 3 + 6)       # LDA #$69, LDX #4, LDY #col, JSR $8107 (STA $10, STX $1A, STY $04, LDA #0, STA $03, RTS)
        s.z[0x10], s.z[0x1A], s.z[0x04], s.z[0x03] = 0x69, 4, col_y, 0
        s.add(2 + 2 + 2 + (6 if col_y == 0x0F else 3) + 3 + 4 + 4)   # LDX #7, LDA #lo, LDY #hi, JSR $8112 (the second time JMP $8112): STX $18, STA $55B2, STY $55B3
        s.z[0x18] = 7
        s.p55b2 = data
        s.add(3)                                            # JMP $55A6
        T.r_blit(s)
        if col_y == 0x0F:
            s.add(0)
    # the second JSR/JMP pair: the first block ends with JSR $8112 (6) whose RTS returns to $80F5, the second with JMP $8112


def r_5850(s):                              # JSR $5850 (demo)
    s.add(6)
    r_5839(s)
    s.add(3 + 2)                            # LDA $E8, BEQ (not taken)
    r_80e3(s)


def r_6e79(s):                              # JSR $6E79: the ghost queues
    s.add(2 + 15 * 14 - 1)                  # LDY #$0E, 15 bytes of LDA $8A44,Y / STA $0800,Y / DEY / BPL
    s.add(2 + 2)                            # LDY #0, LDX #1
    for k in range(128):
        s.add(2 + 5 + 5 + 5 + 2 + 2 + 2 + 5 + 5 + 5 + 2 + 2)
        s.add(3 if k < 127 else 2)
    s.add(6)


def r_53e6(s):                              # JSR $53E6: the start of a life
    s.add(6)
    r_5839(s)
    s.add(2 + 14 * (4 + 4 + 2) + 13 * 3 + 2)    # LDX #$0D, 14 x (LDA $5446,X / STA $60,X / DEX), BPL
    s.add(2)
    s.add(84 * (4 + 5 + 2) + 83 * 3 + 2)        # LDY #$53, 84 x ...
    s.add(2 + 2 + 7 * (5 + 2) + 6 * 3 + 2)      # LDA #0, LDY #6, 7 x (STA $0059,Y / DEY), BPL
    s.add(6)
    r_6e79(s)
    r_53cf(s)
    s.add(2 + 3 + 2 + 3 + 2 + 3 + 3 + 3 + 3)       # $E9, $EA, $EB, $E4, $E7, $1F (LDA #0 then four stores)
    s.add(6 + 3 + 2 + 3 + 6)                    # JSR $6D1E (level 1 -> A = 1)
    s.add(2 + 2)                                # TAX, LDY #$12
    s.add(4 + 5 + 4 + 5 + 4 + 5)                # three bytes of the ghost start table
    s.add(2)                                    # LDY #3
    for k in range(4):
        s.add(2 + 6)                            # LDX #$FF, JSR $6CE8
        T.r_delay(s, 255)
        s.add(2 + (3 if k < 3 else 2))          # DEY, BPL
    s.add(6)
    T.r_poll(s)
    s.add(6)


if __name__ == '__main__':
    s = S(22486410)
    r_5303(s)
    mine = [e for e in s.ev]
    theirs = [e for e in EV if e[1] in ('clear', 'hline', 'vline', 'dot', 'tile', 'blit', 'box', 'img12x3', 'dispatch', 'delay', 'poll', 'mclear') and e[0] >= 22486410]
    # up to the end of the dot pass: the last dot/tile of the pass is the first 'blit' (HUD) minus something; compare the common prefix
    n = len(mine)
    bad = 0
    for a, b in zip(mine, theirs):
        pa = tuple(a[2:]); pb = tuple(b[2:])
        if a[1] == 'delay': pa = pa[:1]; pb = pb[:1]
        if a[0] != b[0] or a[1] != b[1] or (a[1] != 'hline' and pa != pb) or (a[1] == 'hline' and pa[1:3] != pb[1:3]) or False:
            bad += 1
            if bad <= 8: print('diff', a, b)
    print('events mine', n, 'compared', min(n, len(theirs)), 'mismatches', bad, ' model time now', s.t)
