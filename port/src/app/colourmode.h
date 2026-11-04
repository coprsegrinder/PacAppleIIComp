// TASK-034-8 (modernization): the colour mode of the picture. Original Colour is the picture as the colour TV showed it (the default); Native Black & White is the picture as a monochrome
// Apple II monitor showed it: the dots in the green of Apple's own Monitor II / III phosphor, with the half-dot shift (src/video/monochrome.h, docs/MONOCHROME_RESEARCH.md).
#pragma once

#include <string>

namespace pacman::app {

enum class ColourMode { Original, NativeMono };

inline constexpr int kColourModeCount = 2;

inline const char* colourModeKey(ColourMode m) noexcept {   // the settings file's words
    return m == ColourMode::NativeMono ? "mono" : "original";
}

inline const char* colourModeLabel(ColourMode m) noexcept {   // the menu's words
    return m == ColourMode::NativeMono ? "NATIVE BLACK & WHITE" : "ORIGINAL COLOUR";
}

inline bool parseColourMode(const std::string& v, ColourMode& out) {
    for (int i = 0; i < kColourModeCount; ++i)
        if (v == colourModeKey(static_cast<ColourMode>(i))) {
            out = static_cast<ColourMode>(i);
            return true;
        }
    return false;
}

inline ColourMode nextColourMode(ColourMode m, int dir) noexcept {
    return static_cast<ColourMode>(((static_cast<int>(m) + dir) % kColourModeCount + kColourModeCount) % kColourModeCount);
}

}  // namespace pacman::app
