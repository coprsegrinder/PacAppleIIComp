// TASK-034-7 (modernization): where the picture sits when the background has a FRAME around it (the CRT bezel: a monitor housing; the Apple II desktop: a window with its title bar and a
// menu bar above). The frame needs room, so the picture's whole-number scale is the largest one for which picture AND frame fit the window; the frame's size is a fixed number of game
// pixels (so it grows with the picture) and everything is centred. For Black and Ambient Glow nothing changes (display_layout.h).
// The Apple II desktop's window can also be MOVED (dragged by its title bar, its frame, or the picture while the pause menu is closed) and RESIZED (the grow box in its lower right corner;
// the picture stays at a whole-number scale, so resizing steps from scale to scale): FrameState keeps the chosen scale and the position, which the layout applies and keeps inside the
// window. Pure.
#pragma once

#include <algorithm>
#include <cmath>

#include "background.h"
#include "display_layout.h"

namespace pacman::app {

// Frame sizes in GAME pixels (multiplied by the picture's scale): how much room the frame takes at each side of the picture.
struct FrameUnits {
    int left, top, right, bottom;
};

inline FrameUnits frameUnits(Background b) noexcept {
    switch (b) {
    case Background::CrtBezel: return {12, 19, 12, 31};   // the monitor housing around a 4:3 glass: face beside and above it, the chin below (vents, knob, power light); a fixed shape, never stretched to the window
    case Background::Desktop: return {6, 22, 6, 6};       // the desktop beside the window and its frame, the menu bar and the title bar above, the window's lower frame
    default: return {0, 0, 0, 0};
    }
}

inline bool hasFrame(Background b) noexcept { return b == Background::CrtBezel || b == Background::Desktop; }

// What the user chose for the desktop's window (the bezel ignores it): its scale (0 = the largest that fits) and its place as an offset, in game pixels, from the centred place.
struct FrameState {
    int scale = 0;
    float offX = 0.0f, offY = 0.0f;
    bool operator==(const FrameState&) const = default;
};

// the largest whole scale for which picture and frame fit; 0 if even 1x does not fit
inline int framedMaxScale(Background b, int clientW, int clientH) noexcept {
    if (!hasFrame(b) || clientW <= 0 || clientH <= 0) return 0;
    const FrameUnits u = frameUnits(b);
    const int unitsW = kGameW + u.left + u.right, unitsH = kGameH + u.top + u.bottom;
    return (clientW / unitsW < clientH / unitsH) ? clientW / unitsW : clientH / unitsH;
}

// The picture's place at a given scale (centred, plus the desktop's chosen offset, kept so that the whole desktop window stays below the menu bar and inside the window).
inline Viewport framedViewportAt(Background b, int clientW, int clientH, int s, const FrameState& st) noexcept {
    const FrameUnits u = frameUnits(b);
    const int unitsW = kGameW + u.left + u.right, unitsH = kGameH + u.top + u.bottom;
    Viewport v;
    v.scale = s;
    v.w = kGameW * s;
    v.h = kGameH * s;
    v.x = u.left * s + (clientW - unitsW * s) / 2;
    v.y = u.top * s + (clientH - unitsH * s) / 2;
    if (b == Background::Desktop) {
        v.x += static_cast<int>(std::lround(st.offX * static_cast<float>(s)));
        v.y += static_cast<int>(std::lround(st.offY * static_cast<float>(s)));
        const int xmin = u.left / 2 * s, xmax = clientW - v.w - u.right / 2 * s;   // the window's own frame (3 units) stays inside
        const int ymin = (8 + 11) * s, ymax = clientH - v.h - 3 * s;              // below the menu bar (8 units) and the title bar (11)
        if (xmin <= xmax) v.x = (std::clamp)(v.x, xmin, xmax);
        if (ymin <= ymax) v.y = (std::clamp)(v.y, ymin, ymax);
    }
    return v;
}

// The picture's place for this background in a client area. Below 1x (a tiny window) the frame is dropped and the picture is shrunk as without a frame.
inline Viewport computeViewportFor(Background b, int clientW, int clientH, const FrameState& st = FrameState{}) noexcept {
    const int fit = framedMaxScale(b, clientW, clientH);
    if (fit < 1) return computeViewport(clientW, clientH);
    const int s = (b == Background::Desktop && st.scale >= 1) ? (std::min)(st.scale, fit) : fit;
    return framedViewportAt(b, clientW, clientH, s, st);
}

// The offset (game pixels) that puts the picture's top left at (x, y) for a given scale.
inline FrameState desktopStateFor(int clientW, int clientH, int s, int x, int y, int chosenScale) noexcept {
    const Viewport centred = framedViewportAt(Background::Desktop, clientW, clientH, s, FrameState{});
    FrameState st;
    st.scale = chosenScale;
    st.offX = static_cast<float>(x - centred.x) / static_cast<float>(s);
    st.offY = static_cast<float>(y - centred.y) / static_cast<float>(s);
    return st;
}

// ---- the desktop window: its parts, for drawing and for the mouse (the same numbers in both) ----
struct DeskRect {
    int x0, y0, x1, y1;
    bool contains(int x, int y) const noexcept { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

inline DeskRect desktopWindowRect(const Viewport& v) noexcept { return {v.x - 3 * v.scale, v.y - 11 * v.scale, v.x + v.w + 3 * v.scale, v.y + v.h + 3 * v.scale}; }
inline DeskRect desktopZoomBox(const Viewport& v) noexcept {   // right box of the title bar: fits the window to the largest size and centres it
    const DeskRect w = desktopWindowRect(v);
    return {w.x1 - 7 * v.scale, w.y0 + 2 * v.scale, w.x1 - 2 * v.scale, w.y0 + 7 * v.scale};
}
inline DeskRect desktopGrowBox(const Viewport& v) noexcept {   // the lower right corner of the frame: drag it to resize
    const DeskRect w = desktopWindowRect(v);
    return {w.x1 - 3 * v.scale, w.y1 - 3 * v.scale, w.x1, w.y1};
}

enum class DeskHit { None, Move, Grow, Zoom };

// What a click at (x, y) does. With the pause menu open the picture belongs to the menu (only the frame and the title bar move the window).
inline DeskHit desktopHit(const Viewport& v, int x, int y, bool menuOpen) noexcept {
    if (v.scale < 1) return DeskHit::None;
    const DeskRect w = desktopWindowRect(v);
    if (!w.contains(x, y)) return DeskHit::None;
    if (desktopZoomBox(v).contains(x, y)) return DeskHit::Zoom;
    const bool inPicture = x >= v.x && x < v.x + v.w && y >= v.y && y < v.y + v.h;
    const bool corner = x >= w.x1 - 6 * v.scale && y >= w.y1 - 6 * v.scale;   // the grow box and a little of the picture's corner
    if (corner && (!menuOpen || !inPicture)) return DeskHit::Grow;
    if (inPicture) return menuOpen ? DeskHit::None : DeskHit::Move;
    return DeskHit::Move;
}

}  // namespace pacman::app
