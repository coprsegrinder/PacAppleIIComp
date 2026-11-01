"""TASK-031c: reference model of intermission cartoon 1 (levels 2, 15, 28, ...; routine $81AE) written from the disassembly (development tool).
It produces the sequence of drawing calls (not the timing): ('page_clear',), ('hires_on',), ('masked_clear', x, y), ('dispatch', base, x, y),
('big_sprite', x, y, frame_counter). `body` = the original bytes at $4000 (frame pointer tables $96E8 and $9E5C).
Phase 1: Pac-Man (x = $C1, y = $7A, direction $62 = 3) moves left 2 pixels every 32 loops; ghost 1 (x = $E5, y = $7A) follows 2 pixels every 29 loops; each loop ends with a delay.
When Pac-Man's x falls below $21 both are erased and a pause follows. Phase 2: the ghost (x = $38, y = $7A) moves RIGHT 2 pixels every 7 loops while the big Pac-Man
sprite (4 x 20 bytes, y = $7A - $0E = $6C, x from 2) moves right 2 pixels every 6 loops until x >= $A0; the loop counters keep counting from phase 1."""


def word(body, addr):
    return body[addr - 0x4000] | body[addr + 1 - 0x4000] << 8


def pac_base(body, t66, dir62):
    """$79A4: the frame pointer for Pac-Man from the animation counter $66 (low 4 bits kept) and the direction $62."""
    t = t66 & 3
    if t == 0:
        idx = 0
    else:
        a = 1 if t == 3 else t
        carry = a & 1
        idx = dir62 + 1 if carry else dir62 + 5
    return word(body, 0x96E8 + 2 * idx)


def ghost_base(body, e0, flag1d=1):
    """$88A2 with $78 = $79 = 0: X = 2 * ghost number, plus 12 when $1D is not 0."""
    x = 2 * e0 + (12 if flag1d else 0)
    return word(body, 0x9E5C + x)


def cartoon1(body, flag1d=1):
    ev = [('page_clear',), ('hires_on',)]
    px, py, gx, gy = 0xC1, 0x7A, 0xE5, 0x7A
    c66, d62, e0 = 0, 3, 1
    c59, c80 = 1, 1
    # phase 1
    while True:
        c59 -= 1
        if c59 == 0:
            c59 = 0x20
            ev.append(('masked_clear', px - 4, py - 4))
            px -= 2
            if px < 0x21:
                break
            c66 += 1
            c66 &= 0xFF
            base = pac_base(body, c66 & 0x0F, d62)
            c66 &= 0x0F
            ev.append(('dispatch', base, px, py))
        c80 -= 1
        if c80 == 0:
            c80 = 0x1D
            ev.append(('masked_clear', gx - 4, gy - 4))
            gx -= 2
            ev.append(('dispatch', ghost_base(body, e0, flag1d), gx, gy))
    ev.append(('masked_clear', px - 4, py - 4))          # erase Pac-Man at its last x, then the ghost
    ev.append(('masked_clear', gx - 4, gy - 4))
    px = 0
    py -= 0x0E
    gx = 0x38
    # phase 2: $59 and $80 keep counting from phase 1
    while True:
        c59 -= 1
        if c59 == 0:
            c59 = 6
            px += 2
            if px >= 0xA0:
                break
            ev.append(('big_sprite', px, py, c66))
            c66 = (c66 + 1) & 0xFF
        c80 -= 1
        if c80 == 0:
            c80 = 7
            ev.append(('masked_clear', gx - 4, gy - 4))
            gx += 2
            ev.append(('dispatch', ghost_base(body, e0, flag1d), gx, gy))
    return ev
