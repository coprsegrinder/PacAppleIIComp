// TASK-034-7 (modernization): the pictures of the two framed backgrounds, drawn in code (no outside assets, no licence question): the Apple II CRT monitor bezel and the Apple II desktop.
// Each is drawn for one window size and picture place (the frame follows the picture at every size and every window shape) into a 0xAARRGGBB image the size of the window; the
// renderer draws it as it is and then the game picture over its viewport. The viewport area of the image is black and is never shown. No Apple logo and no lettering of Apple's
// is used anywhere (trademarks); the only text is plain menu and title words in the Apple II character font.
// Pure (no Windows calls); tests/test_gamewindow.cpp.
#pragma once

#include <cstdint>
#include <vector>

#include "../core/textscreen.h"
#include "background.h"
#include "display_layout.h"

namespace pacman::app {

struct ArtInput {
    Background kind = Background::CrtBezel;   // CrtBezel or Desktop
    int width = 0, height = 0;                // the window's client area in pixels
    Viewport viewport;                        // where the game picture sits (framed_layout.h)
    const core::CharacterRom* font = nullptr; // for the desktop's words; without it the words are left out
};

// Fills `argb` (width x height). Returns false for a kind that has no art or an empty size.
bool renderBackgroundArt(const ArtInput& in, std::vector<std::uint32_t>& argb);

}  // namespace pacman::app
