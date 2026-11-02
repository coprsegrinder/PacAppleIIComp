// The Apple II hi-res page (page 1, 280 x 192 pixels, 8,192 bytes) and the drawing operations of the original program
// (docs/GRAPHICS.md). The page is stored exactly like the original stores it, one byte = 7 pixels (bit 0 leftmost) + the palette bit 7,
// in the original interleaved row order, so a drawing operation changes the same bytes as the original does.
// No original data is in this code: sprite, glyph, mask and wall tables are passed in by the caller (from the data pack).
//
// Deliberate differences from the original (documented in docs/GRAPHICS.md):
//  * rows outside 0..191 are skipped (the original would read its row tables beyond their end);
//  * bytes that would land at or beyond offset 8,192 are dropped (the original would write into other memory);
//  * a byte column of 40 or more is NOT clipped: like the original it spills into the next interleaved row, which is the
//    original's edge artifact and keeps the page bytes identical.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pacman::hires {

inline constexpr std::size_t kPageBytes = 0x2000;
inline constexpr int kWidth = 280;
inline constexpr int kHeight = 192;
inline constexpr int kBytesPerRow = 40;
inline constexpr std::uint8_t kBlack = 0x80;  // the original clears the page to $80 (black, palette bit set)

// Offset of the start of row y (0..191) inside the page: the standard Apple II interleave (V: equals the tables at $5B67/$5C27).
constexpr std::size_t rowOffset(int y) noexcept {
    return static_cast<std::size_t>((y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28);
}

// Byte column and pixel position inside the byte of the pixel column x (V: equals the tables at $5A58/$5958 for x = 0..507).
struct Cell {
    int column;
    int shift;
};
constexpr Cell pixelCell(int x) noexcept { return {x / 7, x % 7}; }

class Page {
public:
    Page() { clear(kBlack); }
    void clear(std::uint8_t fill = kBlack) noexcept { data_.fill(fill); }  // routine $6F46
    std::uint8_t byte(std::size_t offset) const noexcept { return offset < kPageBytes ? data_[offset] : std::uint8_t{0}; }
    std::span<const std::uint8_t> bytes() const noexcept { return data_; }
    // Writes; an offset outside the page is dropped.
    void set(std::size_t offset, std::uint8_t v) noexcept {
        if (offset < kPageBytes) data_[offset] = v;
    }
    void orInto(std::size_t offset, std::uint8_t v) noexcept {
        if (offset < kPageBytes) data_[offset] = static_cast<std::uint8_t>(data_[offset] | v);
    }

private:
    std::array<std::uint8_t, kPageBytes> data_{};
};

// ---- single pixels and lines (routines $5913, $5CF0, $5D3D, $5D53) ----
void plotPixel(Page& page, int x, int y) noexcept;  // ORs the pixel's bit in; the palette bit is not touched ($5913)
// Horizontal line from x0 to x1 inclusive on row y ($5CF0): every `step`-th pixel (2 = the wall colour mode, 1 = white mode),
// and every touched byte gets the palette bit set (true) or cleared (false).
void hLine(Page& page, int x0, int x1, int y, int step, bool palette) noexcept;
void vLine(Page& page, int x, int y0, int y1) noexcept;  // one pixel per row from y0 to y1 inclusive ($5D3D)
// The maze walls from the 426-byte table (72 horizontal triples x0, x1, y; 70 vertical triples x, y0, y1). colourMode = flag $1D <> 0:
// the normal game (table $5F69, every second pixel). false = the forced mode $1D = 0 (table $5DBF, adjacent pixels). Returns false if the table is too short.
inline constexpr std::size_t kWallTableBytes = 426;
bool drawWalls(Page& page, std::span<const std::uint8_t> table, bool colourMode) noexcept;

// ---- byte-aligned images (routines $55A6 and $6A3F) ----
// width x rows bytes at (column, row y), row-major. OR blit: page |= data.
void blitOr(Page& page, int column, int y, int width, int rows, std::span<const std::uint8_t> data) noexcept;
// OR then clear the palette bit ($6A3F, "AND $7F" blitter): page = (page | data) & $7F.
void blitOrClearPalette(Page& page, int column, int y, int width, int rows, std::span<const std::uint8_t> data) noexcept;
// Keep the page's pixels, replace the palette bit by the data's: page = (page & $7F) | data (the 12 x 3 fruit images, $6EE2).
void blitMergePalette(Page& page, int column, int y, int width, int rows, std::span<const std::uint8_t> data) noexcept;
// Pick the pre-shifted copy of an image for pixel column x: 7 copies of width x rows bytes one after another (V for the 3 x 10 character frames).
std::span<const std::uint8_t> shiftedCopy(std::span<const std::uint8_t> sevenCopies, int x, int width, int rows) noexcept;

// ---- clears and small objects ----
inline constexpr std::size_t kMaskTableBytes = 28;  // 7 shifts x 4 bytes (tables $69CD and $6A04)
// Masked clear of a 3 x 10 byte block at pixel (x, y): page = (page & mask) | $80 ($69A1). Callers pass x - 4 and y - 4 for a sprite at (x, y).
void maskedClear(Page& page, int x, int y, std::span<const std::uint8_t> maskTable) noexcept;
void boxClear(Page& page, int column, int y) noexcept;  // 12 rows x 8 bytes set to $80 ($696A)
inline constexpr std::size_t kDotTableBytes = 14;       // 7 shifts x 2 bytes (table $61A8)
void smallDot(Page& page, int x, int y, std::span<const std::uint8_t> dotTable) noexcept;  // 2 rows x 2 bytes ORed ($6172)
inline constexpr std::size_t kTileTableBytes = 112;     // 7 shifts x (8 rows x 2 bytes) (table $61CA)
void tile(Page& page, int x, int y, std::span<const std::uint8_t> tileTable) noexcept;     // 8 x 2 bytes ORed ($693A)

// The maze dot pass $6113: walks the 896-byte map (28 columns x 32 cells, column-major; the original loop visits 27 columns and rows 1..30) and draws
// a small dot (cell bit 0 set, dot at pixel (59 + 6c, 11 + 6(r-1))) or, for any other non-zero cell, an energizer tile one pixel up and left of that spot.
inline constexpr std::size_t kMazeMapBytes = 896;
void drawDots(Page& page, std::span<const std::uint8_t> map, std::span<const std::uint8_t> dotTable, std::span<const std::uint8_t> tileTable) noexcept;

// ---- character images through the dispatcher $6A20 and the two cartoon sprites ----
// A 3 x 10 byte image with 7 pre-shifted copies (210 bytes) drawn for a sprite at pixel (x, y): the image goes to (x - 4, y - 4) and the copy for
// (x - 4) mod 7 is used. orMode = flags $1D and $1E both nonzero: plain OR ($55A6); otherwise OR then clear the palette bit ($6A3F).
inline constexpr std::size_t kSevenCopies3x10 = 7 * 30;
void drawImage(Page& page, int x, int y, std::span<const std::uint8_t> sevenCopies, bool orMode) noexcept;
// The cartoon-1 big Pac-Man ($8259): a 4 x 20 byte frame (80 bytes), shifted left (x mod 7) pixels by the original's rotate sequence ($82B1) and then
// STORED (not ORed) at (x / 7, y).
inline constexpr std::size_t kBigSpriteBytes = 80;
void bigSprite(Page& page, int x, int y, std::span<const std::uint8_t> frame) noexcept;
// The cartoon-3 ghost ($7E1F): a 3 x 10 byte image shifted by the rotate sequence at $7E4F ((x - 5) mod 7 times) and then drawn through the
// dispatcher at (x - 1, y), i.e. as a 3 x 10 block at ((x - 5) / 7, y - 4).
inline constexpr std::size_t kImage3x10 = 30;
void shiftedImage(Page& page, int x, int y, std::span<const std::uint8_t> image, bool orMode) noexcept;

// ---- text (routine $42D0) ----
inline constexpr std::size_t kGlyphTableBytes = 33 * 9;  // 33 glyphs of 1 byte x 9 rows at $4553 (index 32 = the special glyph of code $21)
// Draws one string record (column, row, codes..., code with bit 7 set) with the glyph table. Returns the number of record bytes consumed
// (including the terminator), or 0 if the record is not terminated inside `record`.
std::size_t drawString(Page& page, std::span<const std::uint8_t> glyphs, std::span<const std::uint8_t> record) noexcept;

}  // namespace pacman::hires
