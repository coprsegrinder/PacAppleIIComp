#include "monochrome.h"

#include <cmath>

namespace pacman::video {

namespace {

inline std::size_t at(int pos, int y) { return static_cast<std::size_t>(y) * kDotsW + static_cast<std::size_t>(pos); }

inline void lit(std::span<std::uint8_t> dots, int pos, int y) {
    if (pos >= 0 && pos < kDotsW) dots[at(pos, y)] = 1;
}

bool enough(std::size_t size) { return size >= static_cast<std::size_t>(kDotsW) * kDotsH; }

}  // namespace

void hiresDots(const hires::Page& page, std::span<std::uint8_t> dots) noexcept {
    if (!enough(dots.size())) return;
    for (std::size_t i = 0; i < static_cast<std::size_t>(kDotsW) * kDotsH; ++i) dots[i] = 0;
    for (int y = 0; y < hires::kHeight; ++y) {
        const std::size_t base = hires::rowOffset(y);
        for (int col = 0; col < hires::kBytesPerRow; ++col) {
            const std::uint8_t b = page.byte(base + static_cast<std::size_t>(col));
            const int shift = (b & 0x80) ? 1 : 0;   // the palette bit: the whole byte half a dot later
            for (int k = 0; k < 7; ++k)
                if ((b >> k) & 1) {
                    const int pos = 2 * (col * 7 + k) + shift;
                    lit(dots, pos, y);
                    lit(dots, pos + 1, y);
                }
        }
    }
}

void bitmapDots(std::span<const std::uint8_t> bitmap, std::span<std::uint8_t> dots) noexcept {
    if (!enough(dots.size()) || bitmap.size() < static_cast<std::size_t>(280) * kDotsH) return;
    for (int y = 0; y < kDotsH; ++y)
        for (int x = 0; x < 280; ++x) {
            const std::uint8_t v = bitmap[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] ? 1 : 0;
            dots[at(2 * x, y)] = v;
            dots[at(2 * x + 1, y)] = v;
        }
}

void loresDots(std::span<const std::uint8_t> page, std::span<std::uint8_t> dots) noexcept {
    if (!enough(dots.size()) || page.size() < 1024) return;
    for (int r = 0; r < 24; ++r) {
        const std::size_t base = static_cast<std::size_t>((r & 7) * 0x80 + (r >> 3) * 0x28);
        for (int c = 0; c < 40; ++c) {
            const std::uint8_t b = page[base + static_cast<std::size_t>(c)];
            for (int y = 0; y < 8; ++y) {
                const int colour = y < 4 ? (b & 15) : (b >> 4);
                for (int p = 0; p < 14; ++p) {
                    const int pos = c * 14 + p;   // 14 positions per block: the 4 bits go round 3.5 times, so the next block starts half a cycle later
                    dots[at(pos, r * 8 + y)] = static_cast<std::uint8_t>((colour >> (pos & 3)) & 1);
                }
            }
        }
    }
}

void argbDots(std::span<const std::uint32_t> argb, std::span<std::uint8_t> dots) noexcept {
    if (!enough(dots.size()) || argb.size() < static_cast<std::size_t>(280) * kDotsH) return;
    for (int y = 0; y < kDotsH; ++y)
        for (int x = 0; x < 280; ++x) {
            const std::uint32_t p = argb[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)];
            const std::uint8_t v = (p & 0xFFFFFF) != 0 ? 1 : 0;
            dots[at(2 * x, y)] = v;
            dots[at(2 * x + 1, y)] = v;
        }
}

void softenDots(std::span<const std::uint8_t> dots, std::span<std::uint8_t> level) noexcept {
    if (!enough(dots.size()) || !enough(level.size())) return;
    for (int y = 0; y < kDotsH; ++y)
        for (int x = 0; x < kDotsW; ++x) {
            const int left = x > 0 ? dots[at(x - 1, y)] : 0, mid = dots[at(x, y)], right = x + 1 < kDotsW ? dots[at(x + 1, y)] : 0;
            level[at(x, y)] = static_cast<std::uint8_t>(std::lround((0.25 * left + 0.5 * mid + 0.25 * right) * 255.0));
        }
}

std::uint32_t phosphorArgb(std::uint8_t level) noexcept {
    const float k = static_cast<float>(level) / 255.0f;
    const auto byteOf = [](float v) { return static_cast<std::uint32_t>(std::lround(v * 255.0f)); };
    return 0xFF000000u | (byteOf(kPhosphorR * k) << 16) | (byteOf(kPhosphorG * k) << 8) | byteOf(kPhosphorB * k);
}

void phosphorImage(std::span<const std::uint8_t> level, std::span<std::uint32_t> out) noexcept {
    if (!enough(level.size()) || out.size() < static_cast<std::size_t>(kDotsW) * kDotsH) return;
    for (std::size_t i = 0; i < static_cast<std::size_t>(kDotsW) * kDotsH; ++i) out[i] = phosphorArgb(level[i]);
}

void phosphorImage280(std::span<const std::uint8_t> level, std::span<std::uint32_t> out) noexcept {
    if (!enough(level.size()) || out.size() < static_cast<std::size_t>(280) * kDotsH) return;
    for (int y = 0; y < kDotsH; ++y)
        for (int x = 0; x < 280; ++x) out[static_cast<std::size_t>(y) * 280 + static_cast<std::size_t>(x)] = phosphorArgb(static_cast<std::uint8_t>((level[at(2 * x, y)] + level[at(2 * x + 1, y)] + 1) / 2));
}

}  // namespace pacman::video
