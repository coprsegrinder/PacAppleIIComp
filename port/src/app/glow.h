// TASK-034-7 (modernization): the colours of the "Ambient Glow" background. The game picture is reduced to a coarse grid (14 x 10 cells, each the average colour of its part of the
// picture); the renderer spreads it over the bars with a soft fall-off from the picture's edge. Only the bars get it: nothing is added to the picture itself (no bloom).
// The hue of a cell is the hue of its content; its brightness is lifted so that any content shows (a few lit pixels in a cell are enough for a faint colour), empty (black) cells stay
// black. The cells change slowly (time constant 0.25 s) so that the blinking of the game (energizers, the 1UP label, ghosts) does not make the background flicker. Pure.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "display_layout.h"

namespace pacman::app {

inline constexpr int kGlowW = 14, kGlowH = 10;

class GlowField {
public:
    static constexpr double kTimeConstant = 0.25;    // seconds for the cells to cover 63 % of a sudden change
    static constexpr float kLift = 5.0f;             // a cell whose brightest colour component averages 0.2 or more shows at full brightness
    static constexpr float kBlack = 0.02f;           // below this average the cell is black

    // The 280 x 192 ARGB picture and the time since the last call. The first call (or reset) takes the picture as it is.
    void update(const std::uint32_t* argb, double dtSeconds) {
        const double alpha = first_ ? 1.0 : (dtSeconds <= 0.0 ? 0.0 : 1.0 - std::exp(-dtSeconds / kTimeConstant));
        first_ = false;
        for (int j = 0; j < kGlowH; ++j) {
            const int y0 = j * kGameH / kGlowH, y1 = (j + 1) * kGameH / kGlowH;
            for (int i = 0; i < kGlowW; ++i) {
                const int x0 = i * kGameW / kGlowW, x1 = (i + 1) * kGameW / kGlowW;
                double r = 0, g = 0, b = 0;
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x) {
                        const std::uint32_t p = argb[static_cast<std::size_t>(y) * kGameW + static_cast<std::size_t>(x)];
                        r += (p >> 16) & 0xFF;
                        g += (p >> 8) & 0xFF;
                        b += p & 0xFF;
                    }
                const double n = static_cast<double>((x1 - x0) * (y1 - y0)) * 255.0;
                float* c = &avg_[static_cast<std::size_t>(j * kGlowW + i) * 3];
                c[0] += static_cast<float>((r / n - c[0]) * alpha);
                c[1] += static_cast<float>((g / n - c[1]) * alpha);
                c[2] += static_cast<float>((b / n - c[2]) * alpha);
                const float m = (std::max)(c[0], (std::max)(c[1], c[2]));
                float* out = &lifted_[static_cast<std::size_t>(j * kGlowW + i) * 3];
                if (m < kBlack) {
                    out[0] = out[1] = out[2] = 0.0f;
                } else {
                    const float k = (std::min)(1.0f, m * kLift) / m;
                    out[0] = c[0] * k;
                    out[1] = c[1] * k;
                    out[2] = c[2] * k;
                }
                std::uint8_t* px = &bytes_[static_cast<std::size_t>(j * kGlowW + i) * 4];
                px[0] = static_cast<std::uint8_t>(std::lround((std::min)(1.0f, out[2]) * 255.0f));   // B
                px[1] = static_cast<std::uint8_t>(std::lround((std::min)(1.0f, out[1]) * 255.0f));   // G
                px[2] = static_cast<std::uint8_t>(std::lround((std::min)(1.0f, out[0]) * 255.0f));   // R
                px[3] = 255;
            }
        }
    }
    void reset() { first_ = true; avg_.fill(0.0f); lifted_.fill(0.0f); bytes_.fill(0); }

    const std::array<std::uint8_t, kGlowW * kGlowH * 4>& bytes() const noexcept { return bytes_; }   // BGRA, row by row, as the texture the shader reads
    const float* cell(int i, int j) const noexcept { return &lifted_[static_cast<std::size_t>(j * kGlowW + i) * 3]; }   // r, g, b of a cell, 0..1

private:
    bool first_ = true;
    std::array<float, kGlowW * kGlowH * 3> avg_{};
    std::array<float, kGlowW * kGlowH * 3> lifted_{};
    std::array<std::uint8_t, kGlowW * kGlowH * 4> bytes_{};
};

}  // namespace pacman::app
