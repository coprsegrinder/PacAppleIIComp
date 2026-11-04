// TASK-034-0/1 (modernization): where the game picture sits in a window or on a screen. Pure arithmetic, no Windows calls (tested in tests/test_gamewindow.cpp).
// The picture is 280 x 192 (1.458:1) and is never stretched to the shape of the display: it is drawn at the largest WHOLE-NUMBER scale that fits (nearest-neighbour, so every Apple
// pixel is an exact block), centred, and everything else is bars. On a display too small for 1x the picture is shrunk by the largest fraction that fits (still 280:192).
#pragma once

#include <algorithm>
#include <cmath>

namespace pacman::app {

inline constexpr int kGameW = 280;
inline constexpr int kGameH = 192;

struct Viewport {
    int x = 0, y = 0, w = 0, h = 0;
    int scale = 0;          // whole-number scale, or 0 when the picture is smaller than 1x (fractional fit)
    bool empty() const noexcept { return w <= 0 || h <= 0; }
};

inline Viewport computeViewport(int clientW, int clientH) noexcept {
    Viewport v;
    if (clientW <= 0 || clientH <= 0) return v;
    const int s = (std::min)(clientW / kGameW, clientH / kGameH);
    if (s >= 1) {
        v.scale = s;
        v.w = kGameW * s;
        v.h = kGameH * s;
    } else {
        const double f = (std::min)(static_cast<double>(clientW) / kGameW, static_cast<double>(clientH) / kGameH);
        v.w = (std::max)(1, static_cast<int>(std::floor(kGameW * f)));
        v.h = (std::max)(1, static_cast<int>(std::floor(kGameH * f)));
    }
    v.x = (clientW - v.w) / 2;
    v.y = (clientH - v.h) / 2;
    return v;
}

}  // namespace pacman::app
