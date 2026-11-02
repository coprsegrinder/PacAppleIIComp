// A plain Windows window (GDI) that shows one hi-res page in colour at an integer scale. No game logic. First version of M3 (TASK-028b).
// GDI is used on purpose: it is enough for 280 x 192 pixels; Direct3D 11 can replace it later as a separate step.
#pragma once

#include <string>

#include "../hires/hires.h"

namespace pacman::app {

struct WindowOptions {
    int scale = 3;                 // whole-number scale, 1..8; each Apple pixel becomes scale x scale screen pixels (no smoothing)
    int seconds = 0;               // 0 = stay open until the user closes it; N = close after N seconds
    std::wstring screenshotPath;   // if not empty: write the window's own client area to this 32-bit BMP, then close (PrintWindow on this window only)
};

// Opens the window and runs until it is closed. Returns 0 on success, non-zero if the window or the screenshot could not be made.
int showPage(const hires::Page& page, const WindowOptions& options);

}  // namespace pacman::app
