#include "colour.h"

#include <array>

namespace pacman::video {

namespace {

struct Row {
    std::array<bool, hires::kWidth> lit{};
    std::array<bool, hires::kWidth> palette{};
};

Row readRow(const hires::Page& page, int y) noexcept {
    Row r;
    const std::size_t base = hires::rowOffset(y);
    for (int col = 0; col < hires::kBytesPerRow; ++col) {
        const std::uint8_t b = page.byte(base + static_cast<std::size_t>(col));
        for (int k = 0; k < 7; ++k) {
            const auto x = static_cast<std::size_t>(col * 7 + k);
            r.lit[x] = ((b >> k) & 1) != 0;
            r.palette[x] = (b & 0x80) != 0;
        }
    }
    return r;
}

PixelClass classifyAt(const Row& r, int x) noexcept {
    const auto i = static_cast<std::size_t>(x);
    if (!r.lit[i]) return PixelClass::Black;
    const bool left = x > 0 && r.lit[i - 1];
    const bool right = x + 1 < hires::kWidth && r.lit[i + 1];
    if (left || right) return PixelClass::White;
    const bool odd = (x & 1) != 0;
    if (!r.palette[i]) return odd ? PixelClass::Green : PixelClass::Violet;
    return odd ? PixelClass::Orange : PixelClass::Blue;
}

}  // namespace

PixelClass classify(const hires::Page& page, int x, int y) noexcept {
    if (x < 0 || x >= hires::kWidth || y < 0 || y >= hires::kHeight) return PixelClass::Black;
    return classifyAt(readRow(page, y), x);
}

void renderArgb(const hires::Page& page, std::span<std::uint32_t> out) noexcept {
    if (out.size() < static_cast<std::size_t>(hires::kWidth) * hires::kHeight) return;
    for (int y = 0; y < hires::kHeight; ++y) {
        const Row r = readRow(page, y);
        for (int x = 0; x < hires::kWidth; ++x) {
            PixelClass c = classifyAt(r, x);
            // The solid colour of a pattern of every second pixel (the game draws its horizontal walls and the bodies of the ghosts like that): an unlit pixel between two lit ones of the
            // same colour takes that colour, as on a colour display (a TV or an emulator shows such a pattern as one colour). Two lit neighbours of different colours, or a white one, leave it black.
            if (c == PixelClass::Black && x > 0 && x + 1 < hires::kWidth && r.lit[static_cast<std::size_t>(x) - 1] && r.lit[static_cast<std::size_t>(x) + 1]) {
                const PixelClass l = classifyAt(r, x - 1), rr = classifyAt(r, x + 1);
                if (l == rr && l != PixelClass::White) c = l;
            }
            out[static_cast<std::size_t>(y * hires::kWidth + x)] = colourOf(c);
        }
    }
}

}  // namespace pacman::video
