// TASK-033-4e: the Apple II text screen (40 x 24, text page 2 at $0800) as the original's prompts use it. The original does its text output through the monitor ROM
// (COUT, VTAB, RDKEY); this class has the same effect on the screen memory (the effect is documented behaviour of that ROM: tools/apple2.py stands in for it the same way, and the
// text pages of the original running against that stand-in are what the tests compare). The pictures use the Apple II character generator, which is read at run time from a file
// the user provides (the repository's "Apple II roms" folder); the finished release ships it as a file next to the program (user decision 2026-11-03), the game image is never shipped.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pacman::core {

class TextScreen {
public:
    static constexpr int kCols = 40, kRows = 24;
    TextScreen() { clear(); }
    void clear(std::uint8_t fill = 0xA0) noexcept;        // all 1,024 bytes of the page, cursor to the top left ($4868)
    void setCursor(int col, int row) noexcept { col_ = col; row_ = row; }
    void put(std::uint8_t c) noexcept;                    // COUT: $8D = new line, others are stored at the cursor (no inverse set), line wrap
    void puts(std::span<const std::uint8_t> s) noexcept { for (std::uint8_t c : s) put(c); }
    void assign(const std::array<std::uint8_t, 1024>& memory, int col, int row) noexcept { mem_ = memory; col_ = col; row_ = row; }   // the page of the monitor (monitor.h)
    void clearToEndOfLine() noexcept;                     // the monitor's CLREOL ($FC9C): from the cursor to the end of the line with $A0
    std::uint8_t atCursor() const noexcept { return byte(rowOffset(row_) + static_cast<std::size_t>(col_)); }
    std::uint8_t byte(std::size_t offset) const noexcept { return offset < mem_.size() ? mem_[offset] : std::uint8_t{0}; }
    const std::array<std::uint8_t, 1024>& memory() const noexcept { return mem_; }
    int col() const noexcept { return col_; }
    int row() const noexcept { return row_; }
    static std::size_t rowOffset(int row) noexcept { return static_cast<std::size_t>((row & 7) * 0x80 + (row >> 3) * 0x28); }

private:
    std::array<std::uint8_t, 1024> mem_{};
    int col_ = 0, row_ = 0;
};

// The 64 glyphs of the character generator ROM (8 bytes each; bit 6 = leftmost of 7 pixels). `load` takes the first 512 bytes of the 2 KB dump.
struct CharacterRom {
    std::array<std::uint8_t, 512> glyph{};
    bool loaded = false;
    bool load(const std::string& path);
};

inline constexpr int kTextWidth = 280, kTextHeight = 192;

// 280 x 192 pixels, 1 = lit. Codes $80-$FF normal, $00-$3F inverse, $40-$7F flashing (inverse when `flashPhase` is true). `cursor` (col, row) shows the monitor's cursor
// (RDKEY puts a flashing block there while it waits for a key), -1 = none.
void renderText(const TextScreen& screen, const CharacterRom& rom, bool flashPhase, int cursorCol, int cursorRow, std::vector<std::uint8_t>& pixels);

}  // namespace pacman::core
