#include "screens.h"

namespace pacman::video {

namespace {
constexpr int kW = 280, kH = 192;
}

void renderLoresArgb(std::span<const std::uint8_t> page, std::span<std::uint32_t> out) noexcept {
    if (page.size() < 1024 || out.size() < static_cast<std::size_t>(kW) * kH) return;
    for (int r = 0; r < 24; ++r) {
        const std::size_t base = static_cast<std::size_t>((r & 7) * 0x80 + (r >> 3) * 0x28);
        for (int c = 0; c < 40; ++c) {
            const std::uint8_t b = page[base + static_cast<std::size_t>(c)];
            const std::uint32_t top = kLoresColour[b & 15], bottom = kLoresColour[b >> 4];
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 7; ++x) out[static_cast<std::size_t>(r * 8 + y) * kW + static_cast<std::size_t>(c * 7 + x)] = y < 4 ? top : bottom;
        }
    }
}

void renderBitmapArgb(std::span<const std::uint8_t> pixels, std::span<std::uint32_t> out) noexcept {
    if (pixels.size() < static_cast<std::size_t>(kW) * kH || out.size() < static_cast<std::size_t>(kW) * kH) return;
    for (std::size_t i = 0; i < static_cast<std::size_t>(kW) * kH; ++i) out[i] = pixels[i] ? 0xFFFFFFFFu : 0xFF000000u;
}

}  // namespace pacman::video
