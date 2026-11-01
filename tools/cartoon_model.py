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


def cartoon2(body, flag1d=1):
    """Cartoon 2 ($83D0): Pac-Man (x from $C1, 2 pixels per 40 loops) walks left past a post (two vertical lines at x = $80, $81, rows $7C-$81, redrawn every 40 loops)
    while the ghost (x from $E5, 2 pixels per 33 loops) follows. When the ghost reaches the post region (x < $7B) it tears: each step draws one more vertical line
    from the table $847E (x, first row, last row $7F); when it stops at x = $71 it is redrawn 10 pixels further (the torn piece) with two more lines, and stays there
    while Pac-Man walks on to x < $10. Lines are logged as ('vline', x, row0, row1)."""
    ev = [('page_clear',), ('hires_on',)]
    px, py, gx, gy = 0xC1, 0x7A, 0xE5, 0x7A
    c66, d62, e0, c59, c80 = 0, 3, 1, 1, 1
    tab = [body[0x847E - 0x4000 + i] for i in range(8)]
    while True:
        c59 -= 1
        if c59 == 0:
            c59 = 0x28
            ev.append(('vline', 0x80, 0x7C, 0x81))
            ev.append(('vline', 0x81, 0x7C, 0x81))
            ev.append(('masked_clear', px - 4, py - 4))
            px -= 2
            if px < 0x10:
                return ev
            c66 += 1
            c66 &= 0xFF
            base = pac_base(body, c66 & 0x0F, d62)
            c66 &= 0x0F
            ev.append(('dispatch', base, px, py))
        c80 -= 1
        if c80 != 0:
            continue
        c80 = 0x21
        if gx == 0x71:
            continue
        ev.append(('masked_clear', gx - 4, gy - 4))
        gx -= 2
        ev.append(('dispatch', ghost_base(body, e0, flag1d), gx, gy))
        if gx >= 0x7B:
            continue
        if gx == 0x71:
            ev.append(('masked_clear', gx - 4, gy - 4))
            ev.append(('masked_clear', gx + 10 - 4, gy - 4))
            ev.append(('dispatch', ghost_base(body, e0, flag1d), gx, gy))
            ev.append(('vline', 0x74, 0x7D, 0x7F))
            ev.append(('vline', 0x76, 0x7E, 0x7F))
        else:
            x = gx - 0x73
            c80 = 0xA0
            ev.append(('vline', tab[x], tab[x + 1], 0x7F))


def oracle_calls(events):
    """Reduce the oracle's call log (tools/trace_cartoon.py) to the same tuples the models produce."""
    out = []
    for e in events:
        k = e[1]
        if k in ('page_clear', 'hires_on'):
            out.append((k,))
        elif k == 'masked_clear':
            out.append((k, e[14], e[5]))
        elif k == 'dispatch':
            out.append((k, e[2] | e[3] << 8, e[14], e[5]))
        elif k == 'big_sprite':
            out.append((k, e[8], e[9], e[12]))
        elif k in ('vline', 'hline'):
            out.append((k, e[2], e[3], e[15]))
        elif k == 'w7DA9' and e[13] == 0x7E1F:           # entry of the software-shifted image routine: A/Y still hold the image pointer
            out.append(('shifted_image', e[2] | e[3] << 8))
    return out


def cartoon3(body, flag1d=1):
    """Cartoon 3 ($8486): Pac-Man (x from $C1, 2 pixels per 38 loops) walks left with the ghost behind him (x from $E5, 2 pixels per 33 loops); the ghost drags
    a trail: a vertical line at x + 3, rows $7D-$7F, after every step. When Pac-Man's x falls below $20 the ghost is erased, there is a pause, and both
    come back to the right: Pac-Man from x = $10 (direction 2, 2 pixels per 34 loops) until x >= $B0, the ghost from x = $37 (2 pixels per 39 loops) now drawn as a
    30-byte image taken from one of two pointers ($856C or $854E, chosen by bit 1 of the animation counter $66), shifted by software to the pixel position
    (routine $7E1F: image at x - 5 and y - 4, shift = (x - 5) mod 7, drawn at (x - 1, y) from the buffer $7E70 - 30 x shift) after a masked clear with the mask table $6A04."""
    ev = [('page_clear',), ('hires_on',)]
    px, py, gx, gy = 0xC1, 0x7A, 0xE5, 0x7A
    c66, d62, e0, c59, c80 = 0, 3, 1, 1, 1
    while True:                                             # phase 1
        c59 -= 1
        if c59 == 0:
            c59 = 0x26
            ev.append(('masked_clear', px - 4, py - 4))
            px -= 2
            if px < 0x20:
                break
            c66 += 1
            c66 &= 0xFF
            base = pac_base(body, c66 & 0x0F, d62)
            c66 &= 0x0F
            ev.append(('dispatch', base, px, py))
        c80 -= 1
        if c80 == 0:
            c80 = 0x21
            ev.append(('masked_clear', gx - 4, gy - 4))
            gx -= 2
            ev.append(('dispatch', ghost_base(body, e0, flag1d), gx, gy))
            ev.append(('vline', gx + 3, 0x7D, 0x7F))
    ev.append(('masked_clear', gx - 4, gy - 4))
    gx, px, d62 = 0x37, 0x10, 2
    ptrs = [word(body, 0x854A), word(body, 0x854C)]
    while True:                                             # phase 2
        c59 -= 1
        if c59 == 0:
            c59 = 0x22
            ev.append(('masked_clear', px - 4, py - 4))
            if px >= 0xB0:
                return ev
            px += 2
            c66 += 1
            c66 &= 0xFF
            base = pac_base(body, c66 & 0x0F, d62)
            c66 &= 0x0F
            ev.append(('dispatch', base, px, py))
        c80 -= 1
        if c80 == 0:
            c80 = 0x27
            ev.append(('masked_clear', gx - 5, gy - 4))
            gx += 2
            shift = (gx - 5) % 7
            ev.append(('shifted_image', ptrs[(c66 & 2) >> 1]))
            ev.append(('dispatch', 0x7E70 - 30 * shift, gx - 1, gy))
