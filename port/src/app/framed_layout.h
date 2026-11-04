// TASK-034-7 (modernization): where the picture sits when the background has a FRAME around it (the CRT bezel: a monitor housing; the Apple II desktop: a window with its title bar and a
// menu bar above). The frame needs room, so the picture's whole-number scale is the largest one for which picture AND frame fit the window; the frame's size is a fixed number of game
// pixels (so it grows with the picture) and everything is centred. For Black and Ambient Glow nothing changes (display_layout.h). Pure.
#pragma once

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

// The picture's place for this background in a client area. Below 1x (a tiny window) the frame is dropped and the picture is shrunk as without a frame.
inline Viewport computeViewportFor(Background b, int clientW, int clientH) noexcept {
    if (!hasFrame(b) || clientW <= 0 || clientH <= 0) return computeViewport(clientW, clientH);
    const FrameUnits u = frameUnits(b);
    const int unitsW = kGameW + u.left + u.right, unitsH = kGameH + u.top + u.bottom;
    const int s = (clientW / unitsW < clientH / unitsH) ? clientW / unitsW : clientH / unitsH;
    if (s < 1) return computeViewport(clientW, clientH);
    Viewport v;
    v.scale = s;
    v.w = kGameW * s;
    v.h = kGameH * s;
    v.x = u.left * s + (clientW - unitsW * s) / 2;
    v.y = u.top * s + (clientH - unitsH * s) / 2;
    return v;
}

}  // namespace pacman::app
