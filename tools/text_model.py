"""TASK-027d: reference model of the attract-screen text printer $42D0 (development tool; written from the disassembly).
Entry: record = pointer ($14/$15) + Y. A string record is: x column, y row, then codes, ended by a code with bit 7 set ($FF).
Code $20 = a blank (advance only); code $21 = glyph 32 (image at $4673); codes 1 and 2 = the glyph drawn two rows lower; any other code c = glyph c.
Every glyph is 1 byte wide x 9 rows at $4553 + 9 * c, ORed onto the page (OR blitter $55A6, shift 0). The column advances by 1 after every code.
The delay ($6CE8) and the key poll ($6F5E) after each glyph do not change the picture."""


def row_address(y):
    return 0x2000 + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


def draw_string(page, body, record_addr):
    """page: bytearray(0x2000); body: the original bytes at $4000.. (glyph data); record_addr: address of the record inside body."""
    r = record_addr - 0x4000
    col, row = body[r], body[r + 1]
    i = r + 2
    while body[i] < 0x80:
        c = body[i]
        if c != 0x20:
            y = row + 2 if 0 < c < 3 else row
            base = 0x4673 if c == 0x21 else 0x4553 + 9 * c
            for k in range(9):
                a = row_address(y + k) - 0x2000 + col
                page[a] |= body[base - 0x4000 + k]
        col += 1
        i += 1
    return page
