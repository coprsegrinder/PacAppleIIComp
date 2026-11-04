// TASK-034-5 (modernization): the resolutions of the Resolution setting. With no exclusive full screen, a resolution is the size of the picture area of the WINDOWED mode (the window's
// client area, in real pixels); Borderless Fullscreen always uses the desktop's own resolution. Two sizes are never mixed up:
//   display resolution  = the size the picture is shown at (the window's client area / the back buffer): one of these, or the desktop's, or anything the user dragged the window to
//   game resolution     = 280 x 192, the picture the Apple II made (kGameW x kGameH); it is drawn into the display at a whole-number scale, centred, with bars (display_layout.h)
// Pure arithmetic (tested in tests/test_gamewindow.cpp).
#pragma once

#include <functional>
#include <string>

namespace pacman::app {

struct Resolution {
    int w, h;
    const char* ratio;
};

// In the order of the specification: 21:9, 16:9, 16:10, 4:3.
inline constexpr Resolution kResolutions[] = {
    {2560, 1080, "21:9"}, {3440, 1440, "21:9"},
    {1920, 1080, "16:9"}, {2560, 1440, "16:9"}, {3840, 2160, "16:9"}, {1280, 720, "16:9"}, {1600, 900, "16:9"}, {1366, 768, "16:9"},
    {1680, 1050, "16:10"}, {1920, 1200, "16:10"},
    {1024, 768, "4:3"}, {1280, 960, "4:3"},
};
inline constexpr int kResolutionCount = static_cast<int>(sizeof(kResolutions) / sizeof(kResolutions[0]));

inline int resolutionIndex(int w, int h) noexcept {
    for (int i = 0; i < kResolutionCount; ++i)
        if (kResolutions[i].w == w && kResolutions[i].h == h) return i;
    return -1;
}

// "1920X1080 (16:9)" for a listed size, "CUSTOM 777X555" for any other (the menu's capitals).
inline std::string resolutionLabel(int w, int h) {
    const int i = resolutionIndex(w, h);
    if (i < 0) return "CUSTOM " + std::to_string(w) + "X" + std::to_string(h);
    return std::to_string(w) + "X" + std::to_string(h) + " (" + kResolutions[i].ratio + ")";
}

// The next (dir = +1) or previous (dir = -1) entry that `fits` (the display can show it), going round the list; from a size that is not in the list, the first / last that fits.
// Returns -1 if nothing fits.
inline int nextResolution(int current, int dir, const std::function<bool(int)>& fits) {
    for (int step = 1; step <= kResolutionCount; ++step) {
        const int i = current < 0 ? (dir > 0 ? step - 1 : kResolutionCount - step) : ((current + dir * step) % kResolutionCount + kResolutionCount) % kResolutionCount;
        if (fits(i)) return i;
    }
    return -1;
}

}  // namespace pacman::app
