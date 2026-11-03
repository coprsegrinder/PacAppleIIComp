#include "textscreen.h"

#include <fstream>
#include <iterator>

namespace pacman::core {

void TextScreen::clear(std::uint8_t fill) noexcept {
    mem_.fill(fill);
    col_ = 0;
    row_ = 0;
}

void TextScreen::put(std::uint8_t c) noexcept {
    if (c == 0x8D) {                 // carriage return: start of the line, then line feed
        col_ = 0;
        ++row_;
    } else if (c >= 0xA0 || c < 0x80) {
        mem_[rowOffset(row_ < kRows ? row_ : kRows - 1) + static_cast<std::size_t>(col_)] = c;
        if (++col_ >= kCols) {
            col_ = 0;
            ++row_;
        }
    }                                // other control codes: no visible effect
    if (row_ >= kRows) {             // scroll
        for (int r = 0; r + 1 < kRows; ++r)
            for (int x = 0; x < kCols; ++x) mem_[rowOffset(r) + static_cast<std::size_t>(x)] = mem_[rowOffset(r + 1) + static_cast<std::size_t>(x)];
        for (int x = 0; x < kCols; ++x) mem_[rowOffset(kRows - 1) + static_cast<std::size_t>(x)] = 0xA0;
        row_ = kRows - 1;
    }
}

bool CharacterRom::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    const std::vector<char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < glyph.size()) return false;
    for (std::size_t i = 0; i < glyph.size(); ++i) glyph[i] = static_cast<std::uint8_t>(d[i]);
    loaded = true;
    return true;
}

void renderText(const TextScreen& screen, const CharacterRom& rom, bool flashPhase, int cursorCol, int cursorRow, std::vector<std::uint8_t>& pixels) {
    pixels.assign(static_cast<std::size_t>(kTextWidth) * kTextHeight, 0);
    for (int r = 0; r < TextScreen::kRows; ++r) {
        for (int c = 0; c < TextScreen::kCols; ++c) {
            std::uint8_t code = screen.byte(TextScreen::rowOffset(r) + static_cast<std::size_t>(c));
            if (c == cursorCol && r == cursorRow) code = static_cast<std::uint8_t>((code & 0x3F) | 0x40);   // RDKEY: the flashing version of the character under the cursor
            const bool inverse = code < 0x40 || (code < 0x80 && flashPhase);
            const std::size_t g = static_cast<std::size_t>(code & 0x3F) * 8;
            for (int y = 0; y < 8; ++y) {
                const std::uint8_t bits = rom.loaded ? rom.glyph[g + static_cast<std::size_t>(y)] : std::uint8_t{0};
                for (int x = 0; x < 7; ++x) {
                    const bool lit = ((bits >> (6 - x)) & 1) != 0;
                    pixels[static_cast<std::size_t>(r * 8 + y) * kTextWidth + static_cast<std::size_t>(c * 7 + x)] = (lit != inverse) ? 1 : 0;
                }
            }
        }
    }
}

}  // namespace pacman::core
