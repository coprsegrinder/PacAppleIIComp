"""TASK-027d: reference model of the maze wall routine $5D53 (with $5CF0 and $5D3D), written from the disassembly (development tool).
Input: the 426-byte wall table (72 horizontal triples x0, x1, y, then 70 vertical triples x, y0, y1) and the flag $1D.
Output: the 8,192-byte hi-res page, starting from a page filled with $80 (the page clear $6F46 runs just before).
Horizontal line (y fixed): pixels x0, x0+s, x0+2s ... while <= x1, where s = 2 if $1D <> 0 else 1; every touched byte gets bit 7 set ($1E = $80).
Vertical line: one pixel at x for every row y0..y1 (the palette bit is not touched)."""


def row_address(y):
    return 0x2000 + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


def wall_page(table, flag_1d):
    page = bytearray([0x80]) * 0x2000
    step = 2 if flag_1d else 1

    def plot(x, y, set_palette):
        a = row_address(y) - 0x2000 + x // 7
        page[a] |= 1 << (x % 7)
        if set_palette:
            page[a] = (page[a] & 0x7F) | 0x80
    for i in range(72):
        x0, x1, y = table[3 * i: 3 * i + 3]
        x = x0
        while x <= x1:
            plot(x, y, True)
            x += step
    for i in range(72, 142):
        x, y0, y1 = table[3 * i: 3 * i + 3]
        for y in range(y0, y1 + 1):
            plot(x, y, False)
    return bytes(page)
