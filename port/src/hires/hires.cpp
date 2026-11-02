#include "hires.h"

namespace pacman::hires {

namespace {

bool rowOk(int y) noexcept { return y >= 0 && y < kHeight; }

}  // namespace

void plotPixel(Page& page, int x, int y) noexcept {
    if (x < 0 || !rowOk(y)) return;
    const Cell c = pixelCell(x);
    page.orInto(rowOffset(y) + static_cast<std::size_t>(c.column), static_cast<std::uint8_t>(1u << c.shift));
}

void hLine(Page& page, int x0, int x1, int y, int step, bool palette) noexcept {
    if (x0 < 0 || !rowOk(y) || step < 1) return;
    for (int x = x0; x <= x1; x += step) {
        plotPixel(page, x, y);
        const std::size_t off = rowOffset(y) + static_cast<std::size_t>(pixelCell(x).column);
        const std::uint8_t b = page.byte(off);
        page.set(off, palette ? static_cast<std::uint8_t>(b | 0x80) : static_cast<std::uint8_t>(b & 0x7F));
    }
}

void vLine(Page& page, int x, int y0, int y1) noexcept {
    for (int y = y0; y <= y1; ++y) plotPixel(page, x, y);
}

bool drawWalls(Page& page, std::span<const std::uint8_t> table, bool colourMode) noexcept {
    if (table.size() < kWallTableBytes) return false;
    const int step = colourMode ? 2 : 1;
    for (std::size_t i = 0; i < 72; ++i) {
        hLine(page, table[3 * i], table[3 * i + 1], table[3 * i + 2], step, true);
    }
    for (std::size_t i = 72; i < 142; ++i) {
        vLine(page, table[3 * i], table[3 * i + 1], table[3 * i + 2]);
    }
    return true;
}

namespace {

template <typename Op>
void blit(Page& page, int column, int y, int width, int rows, std::span<const std::uint8_t> data, Op op) noexcept {
    if (width <= 0 || rows <= 0 || column < 0) return;
    if (data.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(rows)) return;
    for (int r = 0; r < rows; ++r) {
        if (!rowOk(y + r)) continue;
        const std::size_t base = rowOffset(y + r) + static_cast<std::size_t>(column);
        for (int b = 0; b < width; ++b) {
            const std::size_t off = base + static_cast<std::size_t>(b);
            page.set(off, op(page.byte(off), data[static_cast<std::size_t>(r * width + b)]));
        }
    }
}

}  // namespace

void blitOr(Page& page, int column, int y, int width, int rows, std::span<const std::uint8_t> data) noexcept {
    blit(page, column, y, width, rows, data, [](std::uint8_t p, std::uint8_t d) { return static_cast<std::uint8_t>(p | d); });
}

void blitOrClearPalette(Page& page, int column, int y, int width, int rows, std::span<const std::uint8_t> data) noexcept {
    blit(page, column, y, width, rows, data, [](std::uint8_t p, std::uint8_t d) { return static_cast<std::uint8_t>((p | d) & 0x7F); });
}

void blitMergePalette(Page& page, int column, int y, int width, int rows, std::span<const std::uint8_t> data) noexcept {
    blit(page, column, y, width, rows, data, [](std::uint8_t p, std::uint8_t d) { return static_cast<std::uint8_t>((p & 0x7F) | d); });
}

std::span<const std::uint8_t> shiftedCopy(std::span<const std::uint8_t> sevenCopies, int x, int width, int rows) noexcept {
    const std::size_t size = static_cast<std::size_t>(width) * static_cast<std::size_t>(rows);
    const std::size_t start = static_cast<std::size_t>(pixelCell(x).shift) * size;
    if (x < 0 || start + size > sevenCopies.size()) return {};
    return sevenCopies.subspan(start, size);
}

void maskedClear(Page& page, int x, int y, std::span<const std::uint8_t> maskTable) noexcept {
    if (x < 0 || maskTable.size() < kMaskTableBytes) return;
    const Cell c = pixelCell(x);
    for (int r = 0; r < 10; ++r) {
        if (!rowOk(y + r)) continue;
        const std::size_t base = rowOffset(y + r) + static_cast<std::size_t>(c.column);
        for (int b = 0; b < 3; ++b) {
            const std::size_t off = base + static_cast<std::size_t>(b);
            const std::uint8_t mask = maskTable[static_cast<std::size_t>(c.shift * 4 + b)];
            page.set(off, static_cast<std::uint8_t>((page.byte(off) & mask) | 0x80));
        }
    }
}

void boxClear(Page& page, int column, int y) noexcept {
    for (int r = 0; r < 12; ++r) {
        if (!rowOk(y + r)) continue;
        const std::size_t base = rowOffset(y + r) + static_cast<std::size_t>(column);
        for (int b = 0; b < 8; ++b) page.set(base + static_cast<std::size_t>(b), kBlack);
    }
}

void smallDot(Page& page, int x, int y, std::span<const std::uint8_t> dotTable) noexcept {
    if (x < 0 || dotTable.size() < kDotTableBytes) return;
    const Cell c = pixelCell(x);
    for (int r = 0; r < 2; ++r) {
        if (!rowOk(y + r)) continue;
        const std::size_t base = rowOffset(y + r) + static_cast<std::size_t>(c.column);
        for (int b = 0; b < 2; ++b) page.orInto(base + static_cast<std::size_t>(b), dotTable[static_cast<std::size_t>(c.shift * 2 + b)]);
    }
}

void tile(Page& page, int x, int y, std::span<const std::uint8_t> tileTable) noexcept {
    if (x < 0 || tileTable.size() < kTileTableBytes) return;
    const Cell c = pixelCell(x);
    for (int r = 0; r < 8; ++r) {
        if (!rowOk(y + r)) continue;
        const std::size_t base = rowOffset(y + r) + static_cast<std::size_t>(c.column);
        for (int b = 0; b < 2; ++b) page.orInto(base + static_cast<std::size_t>(b), tileTable[static_cast<std::size_t>(c.shift * 16 + r * 2 + b)]);
    }
}

void drawDots(Page& page, std::span<const std::uint8_t> map, std::span<const std::uint8_t> dotTable, std::span<const std::uint8_t> tileTable) noexcept {
    if (map.size() < kMazeMapBytes || dotTable.size() < kDotTableBytes || tileTable.size() < kTileTableBytes) return;
    int column = 0;
    for (int x = 0x3B; x < 0xDD; x += 6, ++column) {  // $0E: 27 columns, pointer $16/$17 advances 32 bytes per column
        int y = 11;
        for (int row = 1; row < 0x1F; ++row, y += 6) {  // $19 = 1..30, $11 = 11, 17, ...
            const std::uint8_t cell = map[static_cast<std::size_t>(column) * 32 + static_cast<std::size_t>(row)];
            if (cell == 0) continue;
            if (cell & 1) smallDot(page, x, y, dotTable);
            else tile(page, x - 1, y - 1, tileTable);
        }
    }
}

namespace {

// The 6502 shift and rotate instructions on a byte with an explicit carry flag.
struct Cpu {
    bool carry = false;
    void asl(std::uint8_t& b) noexcept {
        carry = (b & 0x80) != 0;
        b = static_cast<std::uint8_t>(b << 1);
    }
    void rol(std::uint8_t& b) noexcept {
        const bool c = carry;
        carry = (b & 0x80) != 0;
        b = static_cast<std::uint8_t>((b << 1) | (c ? 1 : 0));
    }
    void ror(std::uint8_t& b) noexcept {
        const bool c = carry;
        carry = (b & 0x01) != 0;
        b = static_cast<std::uint8_t>((b >> 1) | (c ? 0x80 : 0));
    }
};

}  // namespace

void drawImage(Page& page, int x, int y, std::span<const std::uint8_t> sevenCopies, bool orMode) noexcept {
    const int px = x - 4;
    if (px < 0) return;
    const auto copy = shiftedCopy(sevenCopies, px, 3, 10);
    if (copy.empty()) return;
    const int column = pixelCell(px).column;
    if (orMode) {
        blitOr(page, column, y - 4, 3, 10, copy);
    } else {
        blitOrClearPalette(page, column, y - 4, 3, 10, copy);
    }
}

void bigSprite(Page& page, int x, int y, std::span<const std::uint8_t> frame) noexcept {
    if (x < 0 || frame.size() < kBigSpriteBytes) return;
    std::array<std::uint8_t, kBigSpriteBytes> buf{};
    for (std::size_t i = 0; i < kBigSpriteBytes; ++i) buf[i] = frame[i];
    const Cell c = pixelCell(x);
    for (int n = 0; n < c.shift; ++n) {  // routine $82B1: every row of 4 bytes, last row first
        for (int row = 19; row >= 0; --row) {
            std::uint8_t* r = &buf[static_cast<std::size_t>(row) * 4];
            Cpu cpu;
            cpu.asl(r[0]);
            cpu.asl(r[0]);
            cpu.rol(r[1]);
            cpu.asl(r[1]);
            cpu.rol(r[2]);
            cpu.asl(r[2]);
            cpu.rol(r[3]);
            cpu.ror(r[2]);
            cpu.ror(r[1]);
            cpu.ror(r[0]);
        }
    }
    for (int r = 0; r < 20; ++r) {
        if (!rowOk(y + r)) continue;
        const std::size_t base = rowOffset(y + r) + static_cast<std::size_t>(c.column);
        for (int b = 0; b < 4; ++b) page.set(base + static_cast<std::size_t>(b), buf[static_cast<std::size_t>(r * 4 + b)]);
    }
}

void shiftedImage(Page& page, int x, int y, std::span<const std::uint8_t> image, bool orMode) noexcept {
    const int px = x - 5;
    if (px < 0 || image.size() < kImage3x10) return;
    std::array<std::uint8_t, kImage3x10> buf{};
    for (std::size_t i = 0; i < kImage3x10; ++i) buf[i] = image[i];
    const Cell c = pixelCell(px);
    for (int n = 0; n < c.shift; ++n) {  // routine $7E4F: every row of 3 bytes, last row first
        for (int row = 9; row >= 0; --row) {
            std::uint8_t* r = &buf[static_cast<std::size_t>(row) * 3];
            Cpu cpu;
            cpu.asl(r[0]);
            cpu.asl(r[0]);
            cpu.rol(r[1]);
            cpu.asl(r[1]);
            cpu.rol(r[2]);
            cpu.ror(r[1]);
            cpu.ror(r[0]);
        }
    }
    if (orMode) {
        blitOr(page, c.column, y - 4, 3, 10, buf);
    } else {
        blitOrClearPalette(page, c.column, y - 4, 3, 10, buf);
    }
}

std::size_t drawString(Page& page, std::span<const std::uint8_t> glyphs, std::span<const std::uint8_t> record) noexcept {
    if (glyphs.size() < kGlyphTableBytes || record.size() < 3) return 0;
    int column = record[0];
    const int row = record[1];
    std::size_t i = 2;
    while (i < record.size() && record[i] < 0x80) {
        const std::uint8_t code = record[i];
        if (code != 0x20) {  // $20 = blank: advance only
            const int y = (code > 0 && code < 3) ? row + 2 : row;  // codes 1 and 2 are drawn two rows lower
            const std::size_t glyph = code == 0x21 ? 32 : code;      // $21 = the special glyph (index 32, the image at $4673)
            if (glyph < 33) blitOr(page, column, y, 1, 9, glyphs.subspan(glyph * 9, 9));
        }
        ++column;
        ++i;
    }
    return i < record.size() ? i + 1 : 0;
}

}  // namespace pacman::hires
